// ipkDec.cpp : Defines the entry point for the console application.
//
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string>
#include <stdint.h>

#ifndef WIN32
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#define PATH_DELIMITER '/'
#define SNPRINTF  snprintf
#else
#include <direct.h>
#include "getopt.h"
#define PATH_DELIMITER '\\'
#define SNPRINTF  _snprintf
#endif

#define IPK_MAJOR_VERSION "1"
//v1.1修改以前版本对新压缩的ipk文件解压失败的bug。并且增加linux编译成功
#define IPK_MINOR_VERSION "1" 

//#pragma comment(lib, "frameproc.lib")
using namespace std;
static void usage(const char *program)
{
	const char* verStr = IPK_MAJOR_VERSION "." IPK_MINOR_VERSION;
	printf("Usage: %s [OPTION]\n"
		"Version:%s\n"
		"-h, --help          Help info.\n"
		"-i, --infile        Input ipk file path.[MUST]\n"
		"-o, --outdir        Output file dir.If not specify output dir, use input dir\n"
		"-n, --imgnum        Decode the image specified by number(start from 1)\n"
		"-t, --time          Decode the image specified by time(unit:second)\n" 
		"-v, --version       Display program version.\n"
		"-l, --log           Display detail log info:image number, path, name.\n"
		"\n",
		program,
		verStr
	);

	return;
}

struct image_pack_header_t {
	char fileType[4];		// "PIPK" indicate file type
	uint16_t imageType;		// Thumbnail image type
	uint16_t tileRow;		// Tile row count
	uint16_t tileCol;		// Tile column count
	uint16_t cellImgW;		// Cell image width
	uint16_t cellImgH;		// Cell image height
	uint16_t bigImgCount;	// Big image count(after stitching)
	uint16_t thumbInterval; // Cell image interval
};

struct image_offset_t {
	uint32_t offset;		// Bytes offset from file start
	uint32_t startTime;		// Start seconds of this big image(stitched)
};

// Dir with last '\\', ext with '.'
bool splitFileName(const char* fileName, std::string& dir, std::string& title, std::string& ext); 
// Generate one image
//void extractImage(FILE* fpIpk, const char* dir, const char* title, const char* ext, int number, int offset, uint32_t imgSize);
void extractImage(FILE* fpIpk, const char* dir, const char* title, const char* ext, int number, int offset, uint32_t imgSize, int logflag);

#define FILE_PATH_LEN 256
char logFileName[FILE_PATH_LEN];

int main(int argc, char* argv[])
{
	char psz_infile[FILE_PATH_LEN] = {0};
	char psz_outdir[FILE_PATH_LEN] = {0};
	int specifyTime = -1;
	int specifyNum = -1;
	int logflag = 0;	//默认关闭详细输出提示，为其他项目服务
	int opt = 0, optind = 0;
	struct option lopts[] = {
		{ "help",		 no_argument, NULL, 'h' },
		{ "log",		 no_argument, NULL, 'l' },
		{ "version",     no_argument,       NULL, 'v' },
		{ "infile",      required_argument, NULL, 'i' },
		{ "outdir",      required_argument, NULL, 'o' },
		{ "imgnum",      required_argument, NULL, 'n' },
		{ "time",        required_argument, NULL, 't' },
		{ 0, 0, 0, 0 },
	};

	const char *sopts = "hlvi:o:n:t:";
	const char* verStr = IPK_MAJOR_VERSION "." IPK_MINOR_VERSION"\n";
	int * p_optind = &optind;
	while ((opt = getopt_long(argc, argv, sopts, lopts, p_optind)) != -1) {
		switch (opt) {
		case 'i':
			strncpy(psz_infile, optarg, FILE_PATH_LEN);
			break;
		case 'o':
			strncpy(psz_outdir, optarg, FILE_PATH_LEN);
			break;
		case 'n':
			specifyNum = atoi(optarg);
			break;
		case 't':
			specifyTime = atoi(optarg);			
			break;
		case 'v':
			printf(verStr);
			break;
		case 'l':
			logflag = 1;
			break;
		case 'h':
		default:
			usage(argv[0]);
			return -1;
		}
	}

	if(!(*psz_infile)) {
		printf("Input ipk file is not supplied.\n");
		return -1;
	}
	
	
#ifdef WIN32
	if (strchr(psz_infile, '\\') != NULL){
		sprintf(logFileName, "log_%s.txt", strrchr(psz_infile, '\\') + 1);
	}
	else {
		sprintf(logFileName, "log_%s.txt", psz_infile);
	}
#else
	if (strchr(psz_infile, '/') != NULL){
		sprintf(logFileName, "log_%s.txt", strrchr(psz_infile, '/') + 1);
	}
	else {
		sprintf(logFileName, "log_%s.txt", psz_infile);
	}
#endif

	std::string originDir, fileTitle, fileExt;
	splitFileName(psz_infile, originDir, fileTitle, fileExt);

	if(!(*psz_outdir)) {	// If not specify output dir, use input dir
		strncpy(psz_outdir, originDir.c_str(), originDir.size());
	}
	//std::string outputDir = psz_outdir;
	//if(outputDir.back() != '\\' && outputDir.back() != '/') {
	//	outputDir.push_back(PATH_DELIMITER);
	//}

	//路径名最后无'\\'或者'/'，则根据平台不一样，添加对应的斜杠
	int nStrLen = strlen(psz_outdir);
	if (nStrLen > 0)
	{
		char c_lastChar = psz_infile[nStrLen - 1];
		if (c_lastChar != '\\' && c_lastChar != '/' && nStrLen < (sizeof(psz_outdir)-1))
		{
			psz_outdir[nStrLen] = PATH_DELIMITER;
			psz_outdir[nStrLen + 1] = '\0';
		}
	}	
	
	FILE* fpIpk = fopen(psz_infile, "rb");
	if(!fpIpk) return -1;

	// Get ipk file size
	fseek(fpIpk, 0L, SEEK_END);
	long fileLen = ftell(fpIpk);
	fseek(fpIpk, 0L, SEEK_SET);

	// Read file header
	image_pack_header_t ipkHead = {0};
	fread(&ipkHead, sizeof(ipkHead), 1, fpIpk);
	if(ipkHead.fileType[0] != 'P' || ipkHead.fileType[1] != 'I' ||
	   ipkHead.fileType[2] != 'P' || ipkHead.fileType[3] != 'K') {		// Wrong file format
		return -1;
	}
	if(ipkHead.bigImgCount <= 0) {		// No image file
		return -1;
	}
	
	// Read offset table
	//int* imageSizeArr = new int[ipkHead.bigImgCount];
	uint32_t* imageSizeArr = new uint32_t[ipkHead.bigImgCount];
	image_offset_t* offsetArr = new image_offset_t[ipkHead.bigImgCount];
	int i = 0;
	for(; i<ipkHead.bigImgCount; ++i) {
		fread(&offsetArr[i], sizeof(image_offset_t), 1, fpIpk);
		// Get every image file size according offset
		if(i > 0) {
			imageSizeArr[i-1] = offsetArr[i].offset - offsetArr[i-1].offset;
		}
	}
	// Get last image file size
	imageSizeArr[i-1] = fileLen - offsetArr[i-1].offset;

	// Decode image file
	switch(ipkHead.imageType) {
	case 1: fileExt = ".png"; break;
	case 2: fileExt = ".jpg"; break;
	case 3: fileExt = ".bmp"; break;
	case 4: fileExt = ".gif"; break;
	}
	if (logflag)
	{
		printf("tileRow:[%d]\ntileCol:[%d]\n", ipkHead.tileRow, ipkHead.tileCol);
		printf("cellImgW:[%d]\ncellImgH:[%d]\n", ipkHead.cellImgW, ipkHead.cellImgH);
		printf("all pic num:[%d].\n", ipkHead.bigImgCount);
		printf("thumbInterval:[%d]\n", ipkHead.thumbInterval);		
	}
	else
	{
		printf("%d\n%d\n", ipkHead.tileRow, ipkHead.tileCol);
		printf("%d\n%d\n", ipkHead.cellImgW, ipkHead.cellImgH);
		printf("%d\n", ipkHead.bigImgCount);
		printf("%d\n", ipkHead.thumbInterval);
		FILE * pFile = fopen(logFileName, "w");
		if (pFile)
		{
			fprintf(pFile, "%d\n%d\n", ipkHead.tileRow, ipkHead.tileCol);
			fprintf(pFile, "%d\n%d\n", ipkHead.cellImgW, ipkHead.cellImgH);
			fprintf(pFile, "%d\n", ipkHead.bigImgCount);
			fprintf(pFile, "%d\n", ipkHead.thumbInterval);
			fclose(pFile);
			printf("Successfully open file:[log.txt].\n");
		}
		else
		{
			printf("Can not open file:[log.txt].\n");
			return -1;
		}
		
	}
	
	if(specifyNum == -1 && specifyTime == -1) {		// Decode all images
		for(int j=0; j<ipkHead.bigImgCount; ++j) {
			//extractImage(fpIpk,outputDir.c_str(), fileTitle.c_str(), fileExt.c_str(), j+1, offsetArr[j].offset, imageSizeArr[j]);
			extractImage(fpIpk, psz_outdir, fileTitle.c_str(), fileExt.c_str(), j + 1, offsetArr[j].offset, imageSizeArr[j], logflag);
		}
	} else if(specifyNum > 0) {						// Specify image number
		if(specifyNum <= ipkHead.bigImgCount) {
			//extractImage(fpIpk,outputDir.c_str(), fileTitle.c_str(), fileExt.c_str(), specifyNum, offsetArr[specifyNum-1].offset, imageSizeArr[specifyNum-1]);
			extractImage(fpIpk, psz_outdir, fileTitle.c_str(), fileExt.c_str(), specifyNum, offsetArr[specifyNum - 1].offset, imageSizeArr[specifyNum - 1], logflag);
		}
	} else {										// Specify a time point
		if(ipkHead.bigImgCount > 1) {
			int imgDuration = offsetArr[1].startTime - offsetArr[0].startTime;
			for(int j=0; j<ipkHead.bigImgCount; ++j) {
				if(specifyTime >= offsetArr[j].startTime && 
					specifyTime <= offsetArr[j].startTime + imgDuration) {
					//extractImage(fpIpk,outputDir.c_str(), fileTitle.c_str(), fileExt.c_str(), j+1, offsetArr[j].offset, imageSizeArr[j]);
					extractImage(fpIpk, psz_outdir, fileTitle.c_str(), fileExt.c_str(), j + 1, offsetArr[j].offset, imageSizeArr[j], logflag);
					break;
				}
			}
		} else {	// Only one image
			//extractImage(fpIpk,outputDir.c_str(), fileTitle.c_str(), fileExt.c_str(), 1, offsetArr[0].offset, imageSizeArr[0]);
			extractImage(fpIpk, psz_outdir, fileTitle.c_str(), fileExt.c_str(), 1, offsetArr[0].offset, imageSizeArr[0], logflag);
		}
	}

	// Release buffer
	delete[] imageSizeArr;
	delete[] offsetArr;
	if (logflag)
	{
		printf("Success.\n");
	}
	return 0;
}

bool splitFileName(const char* fileName, std::string& dir, std::string& title, std::string& ext)
{
	std::string originFile(fileName);
	size_t slashIdx = originFile.find_last_of('\\');
	if(slashIdx == originFile.npos) {
		slashIdx = originFile.find_last_of('/');
	}
	
	size_t dotIdx = originFile.find_last_of('.');
	if(dotIdx == originFile.npos) {
		return false;
	}

	if(slashIdx == originFile.npos) {
		dir.clear();	// No dir
		title = originFile.substr(0, dotIdx);
	} else {
		dir = originFile.substr(0, slashIdx+1);
		title = originFile.substr(slashIdx+1, dotIdx-slashIdx-1);
	}
	
	ext = originFile.substr(dotIdx);
	return true;
}

void extractImage(FILE* fpIpk, const char* dir, const char* title, const char* ext, int number, int offset, uint32_t imgSize, int logflag)
{
	
	char idxStr[12] = {0};
	std::string imgFilename = dir;
	imgFilename += title;
	//imgFilename += itoa(number, idxStr, 10);
	SNPRINTF(idxStr, sizeof(idxStr), "%d", number);
	imgFilename += idxStr;
	imgFilename += ext;
	if (logflag)
	{
		printf("[%d]--imgFilename:[%s].\n", number, imgFilename.c_str());
	}
	else
	{
		printf("%s\n", imgFilename.c_str());
		FILE * pFile = fopen(logFileName, "a");
		if (pFile)
		{
			fprintf(pFile, "%s\n", imgFilename.c_str());			
			fclose(pFile);
		}
	}
	

	FILE* fp = fopen(imgFilename.c_str(), "wb");
	if(!fp) return;
	fseek(fpIpk, offset, SEEK_SET);
	char* buf = (char*)malloc(imgSize);
	fread(buf, 1, imgSize, fpIpk);
	fwrite(buf, 1, imgSize, fp);
	fclose(fp);
	free(buf);
}
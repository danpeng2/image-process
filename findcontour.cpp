
#include <stdio.h>
#include <stdlib.h>


#include "mathlib.h"                // DSP 数学函数库
#include "VLIB.h"
#include "IMGLib.h"
#include "string.h"

#include <vector>
//#include "image_edge.h"

using namespace std;
/****************************************************************************/
/*                                                                          */
/*              宏定义                                                      */
/*                                                                          */
/****************************************************************************/
// 软件断点
#define SW_BREAKPOINT   asm(" SWBP 0 ");

typedef struct Point{
    int x,y;
}Point;

typedef struct tagRGBQUAD
{
	unsigned char rgbBlue;
	unsigned char rgbGreen;
	unsigned char rgbRed;
	unsigned char rgbReserved;
}RGBQUAD;

typedef struct tagBITMAPFILEHEADER
{
	unsigned short int		bfType;         // 位图文件的类型，必须为 BM
	unsigned int			bfSize;       	// 文件大小，以字节为单位
	unsigned short int		bfReserverd1; 	// 位图文件保留字，必须为0
	unsigned short int		bfReserverd2; 	// 位图文件保留字，必须为0
	unsigned int 			bfbfOffBits;  	// 位图文件头到数据的偏移量，以字节为单位
}BITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER
{
	int 		biSize;					    // 该结构大小，字节为单位
	int 		biWidth;				    // 图形宽度以象素为单位
	int 		biHeight;				    // 图形高度以象素为单位
	short int 	biPlanes;				    // 目标设备的级别，必须为1
	short int 	biBitcount;				    // 颜色深度，每个象素所需要的位数
	int 		biCompression;			    // 位图的压缩类型
	int 		biSizeImage;			    // 位图的大小，以字节为单位
	int 		biXPelsPermeter;		    // 位图水平分辨率，每米像素数
	int 		biYPelsPermeter;		    // 位图垂直分辨率，每米像素数
	int 		biClrUsed;				    // 位图实际使用的颜色表中的颜色数
	int 		biClrImportant;			    // 位图显示过程中重要的颜色数
}BITMAPINFOHEADER;

typedef struct
{
	BITMAPFILEHEADER file;                  // 文件信息区
	BITMAPINFOHEADER info;                  // 图象信息区
	RGBQUAD *pColorTable;                   // 调色板
	unsigned char *imgBuf;					// 位图数据
}bmp;

// 7×7高斯核
char gaussian_7x7[49] =
{
	0, 0,  0,  0,  0, 0, 0,
	0, 1,  4,  6,  4, 1, 0,
	0, 4, 16, 24, 16, 4, 0,
	0, 6, 24, 36, 24, 6, 0,
	0, 4, 16, 24, 16, 4, 0,
	0, 1,  4,  6,  4, 1, 0,
	0, 0,  0,  0,  0, 0, 0
};
const Point ptOffset[8] =
{
    { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 },
    { -1, 0 }, { -1, -1 }, { 0, -1 }, { 1, -1 }
};
/****************************************************************************/
/*                                                                          */
/*              全局变量                                                    */
/*                                                                          */
/****************************************************************************/
unsigned char *newBmpBuf;
unsigned char newpColorTable[256*4];

/****************************************************************************/
/*                                                                          */
/*              函数声明                                                    */
/*                                                                          */
/****************************************************************************/
bmp ReadBmp(char *bmpName);				// 读取 bmp 图片
int SaveBmp(char *bmpName, bmp *m);		// 保存 bmp 图片

bool DrawContour(unsigned char *pImageData, int nWidth, int nHeight, int nWidthStep, Point& ptstart, vector<int>* ChainCode)
{
    // 轮廓绘制
//    for (vector<Point>::iterator i = ChainCode->begin(); i != ChainCode->end(); i++)
//    {
//        pImageData[nWidthStep * (i->y) + i->x] = 0xFF;
//    }
//    return true;
    Point ptCurrent = ptstart;
    pImageData[nWidthStep * ptCurrent.y + ptCurrent.x] = 0xFF;
    for (vector<int>::iterator i = ChainCode->begin(); i != ChainCode->end(); i++)
    {
        ptCurrent.x += ptOffset[*i].x;
        ptCurrent.y += ptOffset[*i].y;
        pImageData[nWidthStep * ptCurrent.y + ptCurrent.x] = 0xFF;
    }
    return true;
}

bool TracingContour(unsigned char *pImageData, int nWidth, int nHeight, Point& ptstart, vector<int>* pChainCode)
{
    int i = 0;
    int j = 0;
    int k = 0;
    int x = 0;
    int y = 0;
    bool bTracing = true;
    bool close = false;
    Point ptCurrent = ptstart;
    Point ptTemp = { 0, 0 };
    // 清空起始点与链码表

    // 轮廓跟踪
    while (bTracing)
    {
        bTracing = false;
        for (i = 0; i < 8; i++, k++)
        {
            k &= 0x07;
            x = ptCurrent.x + ptOffset[k].x;
            y = ptCurrent.y + ptOffset[k].y;
            if (x >= 0 && x < nWidth && y >= 0 && y < nHeight)
            {
                // 判断是否为轮廓点
                if (pImageData[nWidth * y + x] == 0xFF)
                {
                    for (j = 0; j < 8; j += 2)
                    {
                        ptTemp.x = x + ptOffset[j].x;
                        ptTemp.y = y + ptOffset[j].y;
                        if (ptTemp.x >= 0 && ptTemp.x < nWidth &&
                            ptTemp.y >= 0 && ptTemp.y < nHeight)
                        {
                            if (pImageData[nWidth * ptTemp.y + ptTemp.x] == 0)
                            {
                                bTracing = true;
                                ptCurrent.x = x;
                                ptCurrent.y = y;
                                pChainCode->push_back(k);
                                break;
                            }
                        }
                    }
                }
            }
            if (bTracing)
            {
                // 如果当前点为轮廓起点
                if (ptstart.x == ptCurrent.x && ptstart.y == ptCurrent.y)
                {
                    // 则跟踪完毕
                    bTracing = false;
                    close = true;
                }
                break;
            }
        }
        k += 0x06;
    }
    return close;
}
//
double computer_area(vector<Point> *points)
{
	int point_num = points->size();
	if(point_num < 3)
		return 0.0;
	double area_2 = 0.0;
    for (vector<Point>::iterator i = points->begin(); i != points->end() - 1; i++)
		area_2 += i->x * (i+1)->y - i->y * (i+1)->x;
	return fabs(area_2/2.0);
}

void IMG_sobel(const unsigned char* in,  /* Input image data   */ unsigned char* out,
                        short cols, short rows              /* Image dimensions   */)
{
    int H, O, V, i;
    int i00, i01, i02;
    int i10,      i12;
    int i20, i21, i22;
    int w = cols;

    for (i = 0; i < cols*(rows-2) - 2; i++)
    {
        i00=in[i    ]; i01=in[i    +1]; i02=in[i    +2];
        i10=in[i+  w];                  i12=in[i+  w+2];
        i20=in[i+2*w]; i21=in[i+2*w+1]; i22=in[i+2*w+2];

        H = -   i00 - 2*i01 -   i02 +
            +   i20 + 2*i21 +   i22;

        V = -   i00         +   i02
            - 2*i10         + 2*i12
            -   i20         +   i22;

        O = abs(H) + abs(V);

        if (O > 255) O = 255;
        if (O <58 ) O = 0;
        else
        	O=255;

        out[i + 1] = O;
    }
}

int get_start_point(const unsigned char* in, int width, int height, Point* pt)
{
	for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				unsigned char data=in[width * y+x];
				if (data == 0xFF)
				{
					pt->x = x;
					pt->y = y;
					return 1;
				}
			}
		}
	return 0;
}
/****************************************************************************/
/*                                                                          */
/*              主函数                                                      */
/*                                                                          */
/****************************************************************************/
int main(void)
{
	bmp inImg;				                               // 定义一个结构变量
	bmp outImg;
	printf("\r\nImage Canny Edge Dection Application......\r\n");
	inImg = ReadBmp("../Image/2.bmp");

	int width = inImg.info.biWidth;
	int height = inImg.info.biHeight;
	int pixelNums 	= width*height;
	unsigned char* in 	= (unsigned char*)malloc(sizeof(unsigned char)*pixelNums);
	unsigned char* out 	= (unsigned char*)malloc(sizeof(unsigned char)*pixelNums);
	unsigned char* out2 = (unsigned char*)malloc(sizeof(unsigned char)*pixelNums);
	in =inImg.imgBuf;
	IMG_sobel(in,out,width,height);

	Point ptstart = { 0, 0};
	Point ptCurrent= { 0, 0 };
	vector<int>* pChainCode;
	vector<Point> pointss;
	//bool nocontour = true;
	double area;
	//while(nocontour)
	//{
		//nocontour = false;
		//求初始起点
		while(get_start_point(out, width, height, &ptstart))
		{
			pChainCode->clear();
		    pointss.clear();
		    bool close =TracingContour(out, width, height, ptstart, pChainCode);
		    DrawContour(out2, width, height, width, ptstart,pChainCode);
			pointss.push_back(ptstart);
			ptCurrent = ptstart;
			out[width * ptCurrent.y + ptCurrent.x] = 0x00;
		    for (vector<int>::iterator i = pChainCode->begin(); i != pChainCode->end(); i++)
		        {
		            ptCurrent.x += ptOffset[*i].x;
		            ptCurrent.y += ptOffset[*i].y;
		            pointss.push_back(ptCurrent);
		    		out[width * ptCurrent.y + ptCurrent.x] = 0x00;
		        }
		    printf("pointstar = %d   %d\n",pointss[0].x,pointss[0].y);
		}
//		    if(close)
//		    {
//		        area = computer_area(pointss);
//		        printf("area = %f\n",area);
//		        //if(10000<area<14000)
//		        	DrawContour(out2, width, height, width, pointss);
//		    }
	//}


		    printf("pointend = %d   %d\n",pointss[pointss.size()-1].x,pointss[pointss.size()-1].y);
		    outImg.file=inImg.file;
			outImg.info=inImg.info;
			outImg.pColorTable=inImg.pColorTable;
			outImg.imgBuf=out2;
			SaveBmp("../Image/Out.bmp", &outImg);


	 //软件断点
	SW_BREAKPOINT;
}

/****************************************************************************/
/*                                                                          */
/*              读取 bmp 图片                                               */
/*                                                                          */
/****************************************************************************/
bmp ReadBmp(char *bmpName)
{
	bmp m;			// 定义一个位图结构
	FILE *fp;
	unsigned char *BmpBuf;
	int i;
	int bmpWidth;	// 图像的宽
	int bmpHeight;	// 图像的高
	int biBitCount;	// 图像类型，每像素位数
	int lineByte;

	if((fp=fopen( bmpName, "rb"))==NULL)
	{
		printf( "can't open the bmp imgae.\n ");
		exit(0);
	}
	else
	{
		fread(&m.file.bfType, sizeof(char), 2, fp);				// 类型
		fread(&m.file.bfSize, sizeof(int), 1, fp);				    // 文件长度
		fread(&m.file.bfReserverd1, sizeof(short int), 1, fp);	// 保留字 1
		fread(&m.file.bfReserverd2, sizeof(short int), 1, fp);	// 保留字 2
		fread(&m.file.bfbfOffBits, sizeof(int), 1, fp);			// 偏移量
		fread(&m.info.biSize, sizeof(int), 1, fp);				    // 此结构大小
		fread(&m.info.biWidth, sizeof(int), 1, fp);				// 位图的宽度
		fread(&m.info.biHeight, sizeof(int), 1, fp);			    // 位图的高度
		fread(&m.info.biPlanes, sizeof(short), 1, fp);			    // 目标设备位图数
		fread(&m.info.biBitcount, sizeof(short) ,1, fp);		    // 颜色深度
		fread(&m.info.biCompression, sizeof(int), 1, fp);		    // 位图压缩类型
		fread(&m.info.biSizeImage, sizeof(int), 1, fp);			// 位图大小
		fread(&m.info.biXPelsPermeter, sizeof(int), 1, fp);		// 位图水平分辨率
		fread(&m.info.biYPelsPermeter, sizeof(int), 1, fp);		// 位图垂直分辨率
		fread(&m.info.biClrUsed, sizeof(int), 1, fp);			    // 位图实际使用颜色数
		fread(&m.info.biClrImportant, sizeof(int), 1, fp);		    // 位图显示中比较重要颜色数
	}

	// 获取图像宽、高、每像素所占位数等信息
	bmpWidth = m.info.biWidth;
	bmpHeight = m.info.biHeight;
	biBitCount = m.info.biBitcount;

	// 定义变量，计算图像每行像素所占的字节数（必须是4的倍数）
	lineByte = (bmpWidth*biBitCount/8+3)/4*4;

	// 灰度图像有颜色表，且颜色表表项为256
	if(biBitCount == 8)
	{
		// 分配内存空间
		m.pColorTable = (RGBQUAD *)malloc(256*4);
		// 申请颜色表所需要的空间，读颜色表进内存
		fread(m.pColorTable, sizeof(RGBQUAD), 256, fp);
	}

	// 分配内存空间
	m.imgBuf = (unsigned char *)malloc(m.info.biSizeImage);
	BmpBuf = (unsigned char *)malloc(m.info.biSizeImage);

	// 读位图数据
	fread(BmpBuf, lineByte * bmpHeight, 1, fp);

	// 将读到的数据垂直镜像翻转
	for(i = 0; i < bmpHeight; i++)
		memcpy((void *)(m.imgBuf + lineByte * i),
				(void *)(BmpBuf + lineByte * (bmpHeight - i - 1)), lineByte);
	// 关闭文件
	fclose(fp);

	return m;
}

/****************************************************************************/
/*                                                                          */
/*              保存 bmp 图片                                               */
/*                                                                          */
/****************************************************************************/
int SaveBmp(char *bmpName, bmp *m)
{
	unsigned char *BmpBuf;
	int i;
	int bmpWidth;
	int bmpHeight;
	int biBitCount;
	RGBQUAD *pColorTable;

	bmpWidth = m->info.biWidth;
	bmpHeight = m->info.biHeight;
	biBitCount = m->info.biBitcount;
	pColorTable = m->pColorTable;

	// 如果位图数据指针为0，则没有数据传入，函数返回
	if(!m->imgBuf)
		return 0;

	// 待存储图像数据每行字节数为4的倍数
	int lineByte = (bmpWidth * biBitCount / 8 + 3) / 4 * 4;
	//以二进制写的方式打开文件
	FILE *fp = fopen(bmpName,"wb");
	if(fp == 0)
		return 0;

	fwrite(&m->file.bfType, sizeof(short), 1, fp);
	fwrite(&m->file.bfSize, sizeof(int), 1, fp);
	fwrite(&m->file.bfReserverd1, sizeof(short int), 1, fp);
	fwrite(&m->file.bfReserverd2, sizeof(short int), 1, fp);
	fwrite(&m->file.bfbfOffBits, sizeof(int), 1, fp);
	fwrite(&m->info.biSize, sizeof(int), 1, fp);
	fwrite(&m->info.biWidth, sizeof(int), 1, fp);
	fwrite(&m->info.biHeight, sizeof(int), 1, fp);
	fwrite(&m->info.biPlanes, sizeof(short), 1, fp);
	fwrite(&m->info.biBitcount, sizeof(short), 1, fp);
	fwrite(&m->info.biCompression, sizeof(int), 1, fp);
	fwrite(&m->info.biSizeImage, sizeof(int), 1, fp);
	fwrite(&m->info.biXPelsPermeter, sizeof(int), 1, fp);
	fwrite(&m->info.biYPelsPermeter, sizeof(int), 1, fp);
	fwrite(&m->info.biClrUsed, sizeof(int), 1, fp);
	fwrite(&m->info.biClrImportant, sizeof(int), 1, fp);

	// 如果灰度图像，有颜色表，写入文件
	if(biBitCount==8)
		fwrite(pColorTable, sizeof(RGBQUAD)*256, 1, fp);

	// 分配数据缓冲
	BmpBuf = (unsigned char *)malloc(m->info.biSizeImage);
	// 将位图数据垂直镜像翻转再写入
	for(i = 0; i < bmpHeight; i++)
		memcpy((void *)(BmpBuf + lineByte * i),
				(void *)(m->imgBuf + lineByte * (bmpHeight - i - 1)), lineByte);

	// 写位图数据进文件
	fwrite(BmpBuf, bmpHeight*lineByte, 1, fp);

	// 关闭文件
	fclose(fp);

	return 1;
}



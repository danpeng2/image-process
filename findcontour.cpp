
#include <stdio.h>
#include <stdlib.h>


#include "mathlib.h"                // DSP ��ѧ������
#include "VLIB.h"
#include "IMGLib.h"
#include "string.h"

#include <vector>
//#include "image_edge.h"

using namespace std;
/****************************************************************************/
/*                                                                          */
/*              �궨��                                                      */
/*                                                                          */
/****************************************************************************/
// ����ϵ�
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
	unsigned short int		bfType;         // λͼ�ļ������ͣ�����Ϊ BM
	unsigned int			bfSize;       	// �ļ���С�����ֽ�Ϊ��λ
	unsigned short int		bfReserverd1; 	// λͼ�ļ������֣�����Ϊ0
	unsigned short int		bfReserverd2; 	// λͼ�ļ������֣�����Ϊ0
	unsigned int 			bfbfOffBits;  	// λͼ�ļ�ͷ�����ݵ�ƫ���������ֽ�Ϊ��λ
}BITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER
{
	int 		biSize;					    // �ýṹ��С���ֽ�Ϊ��λ
	int 		biWidth;				    // ͼ�ο��������Ϊ��λ
	int 		biHeight;				    // ͼ�θ߶�������Ϊ��λ
	short int 	biPlanes;				    // Ŀ���豸�ļ��𣬱���Ϊ1
	short int 	biBitcount;				    // ��ɫ��ȣ�ÿ����������Ҫ��λ��
	int 		biCompression;			    // λͼ��ѹ������
	int 		biSizeImage;			    // λͼ�Ĵ�С�����ֽ�Ϊ��λ
	int 		biXPelsPermeter;		    // λͼˮƽ�ֱ��ʣ�ÿ��������
	int 		biYPelsPermeter;		    // λͼ��ֱ�ֱ��ʣ�ÿ��������
	int 		biClrUsed;				    // λͼʵ��ʹ�õ���ɫ���е���ɫ��
	int 		biClrImportant;			    // λͼ��ʾ��������Ҫ����ɫ��
}BITMAPINFOHEADER;

typedef struct
{
	BITMAPFILEHEADER file;                  // �ļ���Ϣ��
	BITMAPINFOHEADER info;                  // ͼ����Ϣ��
	RGBQUAD *pColorTable;                   // ��ɫ��
	unsigned char *imgBuf;					// λͼ����
}bmp;

// 7��7��˹��
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
/*              ȫ�ֱ���                                                    */
/*                                                                          */
/****************************************************************************/
unsigned char *newBmpBuf;
unsigned char newpColorTable[256*4];

/****************************************************************************/
/*                                                                          */
/*              ��������                                                    */
/*                                                                          */
/****************************************************************************/
bmp ReadBmp(char *bmpName);				// ��ȡ bmp ͼƬ
int SaveBmp(char *bmpName, bmp *m);		// ���� bmp ͼƬ

bool DrawContour(unsigned char *pImageData, int nWidth, int nHeight, int nWidthStep, Point& ptstart, vector<int>* ChainCode)
{
    // ��������
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
    // �����ʼ���������

    // ��������
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
                // �ж��Ƿ�Ϊ������
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
                // �����ǰ��Ϊ�������
                if (ptstart.x == ptCurrent.x && ptstart.y == ptCurrent.y)
                {
                    // ��������
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
/*              ������                                                      */
/*                                                                          */
/****************************************************************************/
int main(void)
{
	bmp inImg;				                               // ����һ���ṹ����
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
		//���ʼ���
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


	 //����ϵ�
	SW_BREAKPOINT;
}

/****************************************************************************/
/*                                                                          */
/*              ��ȡ bmp ͼƬ                                               */
/*                                                                          */
/****************************************************************************/
bmp ReadBmp(char *bmpName)
{
	bmp m;			// ����һ��λͼ�ṹ
	FILE *fp;
	unsigned char *BmpBuf;
	int i;
	int bmpWidth;	// ͼ��Ŀ�
	int bmpHeight;	// ͼ��ĸ�
	int biBitCount;	// ͼ�����ͣ�ÿ����λ��
	int lineByte;

	if((fp=fopen( bmpName, "rb"))==NULL)
	{
		printf( "can't open the bmp imgae.\n ");
		exit(0);
	}
	else
	{
		fread(&m.file.bfType, sizeof(char), 2, fp);				// ����
		fread(&m.file.bfSize, sizeof(int), 1, fp);				    // �ļ�����
		fread(&m.file.bfReserverd1, sizeof(short int), 1, fp);	// ������ 1
		fread(&m.file.bfReserverd2, sizeof(short int), 1, fp);	// ������ 2
		fread(&m.file.bfbfOffBits, sizeof(int), 1, fp);			// ƫ����
		fread(&m.info.biSize, sizeof(int), 1, fp);				    // �˽ṹ��С
		fread(&m.info.biWidth, sizeof(int), 1, fp);				// λͼ�Ŀ��
		fread(&m.info.biHeight, sizeof(int), 1, fp);			    // λͼ�ĸ߶�
		fread(&m.info.biPlanes, sizeof(short), 1, fp);			    // Ŀ���豸λͼ��
		fread(&m.info.biBitcount, sizeof(short) ,1, fp);		    // ��ɫ���
		fread(&m.info.biCompression, sizeof(int), 1, fp);		    // λͼѹ������
		fread(&m.info.biSizeImage, sizeof(int), 1, fp);			// λͼ��С
		fread(&m.info.biXPelsPermeter, sizeof(int), 1, fp);		// λͼˮƽ�ֱ���
		fread(&m.info.biYPelsPermeter, sizeof(int), 1, fp);		// λͼ��ֱ�ֱ���
		fread(&m.info.biClrUsed, sizeof(int), 1, fp);			    // λͼʵ��ʹ����ɫ��
		fread(&m.info.biClrImportant, sizeof(int), 1, fp);		    // λͼ��ʾ�бȽ���Ҫ��ɫ��
	}

	// ��ȡͼ����ߡ�ÿ������ռλ������Ϣ
	bmpWidth = m.info.biWidth;
	bmpHeight = m.info.biHeight;
	biBitCount = m.info.biBitcount;

	// �������������ͼ��ÿ��������ռ���ֽ�����������4�ı�����
	lineByte = (bmpWidth*biBitCount/8+3)/4*4;

	// �Ҷ�ͼ������ɫ������ɫ�����Ϊ256
	if(biBitCount == 8)
	{
		// �����ڴ�ռ�
		m.pColorTable = (RGBQUAD *)malloc(256*4);
		// ������ɫ������Ҫ�Ŀռ䣬����ɫ����ڴ�
		fread(m.pColorTable, sizeof(RGBQUAD), 256, fp);
	}

	// �����ڴ�ռ�
	m.imgBuf = (unsigned char *)malloc(m.info.biSizeImage);
	BmpBuf = (unsigned char *)malloc(m.info.biSizeImage);

	// ��λͼ����
	fread(BmpBuf, lineByte * bmpHeight, 1, fp);

	// �����������ݴ�ֱ����ת
	for(i = 0; i < bmpHeight; i++)
		memcpy((void *)(m.imgBuf + lineByte * i),
				(void *)(BmpBuf + lineByte * (bmpHeight - i - 1)), lineByte);
	// �ر��ļ�
	fclose(fp);

	return m;
}

/****************************************************************************/
/*                                                                          */
/*              ���� bmp ͼƬ                                               */
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

	// ���λͼ����ָ��Ϊ0����û�����ݴ��룬��������
	if(!m->imgBuf)
		return 0;

	// ���洢ͼ������ÿ���ֽ���Ϊ4�ı���
	int lineByte = (bmpWidth * biBitCount / 8 + 3) / 4 * 4;
	//�Զ�����д�ķ�ʽ���ļ�
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

	// ����Ҷ�ͼ������ɫ��д���ļ�
	if(biBitCount==8)
		fwrite(pColorTable, sizeof(RGBQUAD)*256, 1, fp);

	// �������ݻ���
	BmpBuf = (unsigned char *)malloc(m->info.biSizeImage);
	// ��λͼ���ݴ�ֱ����ת��д��
	for(i = 0; i < bmpHeight; i++)
		memcpy((void *)(BmpBuf + lineByte * i),
				(void *)(m->imgBuf + lineByte * (bmpHeight - i - 1)), lineByte);

	// дλͼ���ݽ��ļ�
	fwrite(BmpBuf, bmpHeight*lineByte, 1, fp);

	// �ر��ļ�
	fclose(fp);

	return 1;
}



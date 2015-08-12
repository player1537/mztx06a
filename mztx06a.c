#include "bcm2835.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/timeb.h>

#include "ascii hex(8x16).h"
#include "GB2312.h"

#define RGB565(r,g,b)((r >> 3) << 11 | (g >> 2) << 5 | ( b >> 3))
#define BCM2708SPI
#define ROTATE90

#define RGB565_MASK_RED   0xF800
#define RGB565_MASK_GREEN 0x07E0
#define RGB565_MASK_BLUE  0x001F

#define MAXSINGLEPIXELS 300

#ifdef ROTATE90
#define XP    0x0201
#define YP    0x0200
#define XS    0x0212
#define YS    0x0210
#define XE    0x0213
#define YE    0x0211
#define MAX_X    319
#define MAX_Y    239
#else

#define XP    0x0200
#define YP    0x0201
#define XS    0x0210
#define YS    0x0212
#define XE    0x0211
#define YE    0x0213
#define MAX_X    239
#define MAX_Y    319
#endif

#define SPICS  RPI_GPIO_P1_24  //GPIO08
#define SPIRS  RPI_GPIO_P1_22  //GPIO25
#define SPIRST RPI_GPIO_P1_16  //GPIO23
#define SPISCL RPI_GPIO_P1_23  //GPIO11
#define SPISCI RPI_GPIO_P1_19  //GPIO10
#define LCDPWM RPI_GPIO_P1_12  //GPIO18
#if 0
#define LCD_CS_CLR  bcm2835_gpio_write(SPICS,0)
#define LCD_RS_CLR  bcm2835_gpio_write(SPIRS,0)
#define LCD_RST_CLR bcm2835_gpio_write(SPIRST,0)
#define LCD_SCL_CLR bcm2835_gpio_write(SPISCL,0)
#define LCD_SCI_CLR  bcm2835_gpio_write(SPISCI,0)

#define LCD_CS_SET  bcm2835_gpio_write(SPICS,1)
#define LCD_RS_SET  bcm2835_gpio_write(SPIRS,1)
#define LCD_RST_SET bcm2835_gpio_write(SPIRST,1)
#define LCD_SCL_SET bcm2835_gpio_write(SPISCL,1)
#define LCD_SCI_SET bcm2835_gpio_write(SPISCI,1)
#else
#define LCD_CS_CLR  bcm2835_gpio_clr(SPICS)
#define LCD_RS_CLR  bcm2835_gpio_clr(SPIRS)
#define LCD_RST_CLR bcm2835_gpio_clr(SPIRST)
#define LCD_SCL_CLR bcm2835_gpio_clr(SPISCL)
#define LCD_SCI_CLR bcm2835_gpio_clr(SPISCI)
#define LCD_PWM_CLR bcm2835_gpio_clr(LCDPWM)

#define LCD_CS_SET  bcm2835_gpio_set(SPICS)
#define LCD_RS_SET  bcm2835_gpio_set(SPIRS)
#define LCD_RST_SET bcm2835_gpio_set(SPIRST)
#define LCD_SCL_SET bcm2835_gpio_set(SPISCL)
#define LCD_SCI_SET bcm2835_gpio_set(SPISCI)
#define LCD_PWM_SET bcm2835_gpio_set(LCDPWM)
#endif

unsigned short color[] = {
	0xf800, 0x07e0, 0x001f, 0xffe0, 0x0000, 0xffff, 0x07ff, 0xf81f
};

/* Image part */
char *value=NULL;
int hsize=0, vsize=0;

void LCD_WR_REG(int index)
{
	char i;
	LCD_CS_CLR;
	LCD_RS_CLR;
#ifdef BCM2708SPI
	bcm2835_spi_transfer(index>>8);
	bcm2835_spi_transfer(index);
#else

	for(i=0;i<16;i++)
	{
		LCD_SCL_CLR;
		if(index&0x8000)
		{
			LCD_SCI_SET;
		}
		else
		{
			LCD_SCI_CLR;
		}
		index=index<<1;
		LCD_SCL_SET;
	}
#endif

	LCD_CS_SET;
}

void LCD_WR_CMD(int index,int val)
{
	char i;
	LCD_CS_CLR;
	LCD_RS_CLR;
#ifdef BCM2708SPI
	bcm2835_spi_transfer(index>>8);
	bcm2835_spi_transfer(index);
#else
	for(i=0;i<16;i++)
	{
		LCD_SCL_CLR;
		if(index&0x8000)
		{
			LCD_SCI_SET;
		}
		else
		{
			LCD_SCI_CLR;
		}
		index=index<<1;
		LCD_SCL_SET;
	}
#endif
	LCD_RS_SET;
#ifdef BCM2708SPI
	bcm2835_spi_transfer(val>>8);
	bcm2835_spi_transfer(val);
#else
	for(i=0;i<16;i++)
	{
		LCD_SCL_CLR;
		if(val&0x8000)
		{
			LCD_SCI_SET;
		}
		else
		{
			LCD_SCI_CLR;
		}
		val=val<<1;
		LCD_SCL_SET;
	}
#endif
	LCD_CS_SET;
}

void inline LCD_WR_Data(int val)
{
	char i;
#ifdef BCM2708SPI
	bcm2835_spi_transfer(val>>8);
	bcm2835_spi_transfer(val);
#else
	for(i=0;i<16;i++)
	{
		LCD_SCL_CLR;
		if(val&0x8000)
		{
			LCD_SCI_SET;
		}
		else
		{
			LCD_SCI_CLR;
		}
		val=val<<1;
		LCD_SCL_SET;
	}
#endif
}


void write_dot(char dx,int dy,int color)
{
	LCD_WR_CMD(0x210,dx);
	LCD_WR_CMD(0x212,dy);
	LCD_WR_CMD(0x211,dx);
	LCD_WR_CMD(0x213,dy);

	LCD_WR_CMD(0x200,dx);
	LCD_WR_CMD(0x201,dy);
	LCD_WR_CMD(0x202,color);
}

void loadFrameBuffer_diff_320()
{
	int  xsize=320, ysize=240;
	unsigned char *buffer;
	FILE *infile=fopen("/dev/fb0","rb");
	long fp;
	int i,j,k;
	unsigned long offset=0;
	int p;
	int r1,g1,b1;
	int r,g,b;
	long minsum=0;
	long nowsum=0;
	int flag;
	int ra,ga,ba;
	int drawmap[2][ysize][xsize];
	int diffmap[ysize][xsize];
	int quickdiffmap [2][MAXSINGLEPIXELS];
	int diffsx, diffsy, diffex, diffey;
	int numdiff=0;
	unsigned int isleep=0;

	buffer = (unsigned char *) malloc(xsize * ysize * 2);
	fseek(infile, 0, 0);

	if (fread (buffer, xsize * ysize *2, sizeof(unsigned char), infile) != 1)
		printf ("Read < %d chars when loading file %s\n", hsize*vsize*3, "ss");

	LCD_WR_CMD(XS,0x00);
	LCD_WR_CMD(YS,0x0000);
	LCD_WR_CMD(XE,MAX_X);
	LCD_WR_CMD(YE,MAX_Y);

	LCD_WR_CMD(XP,0x0000);
	LCD_WR_CMD(YP,0x0000);

	LCD_WR_REG(0x202);
	LCD_CS_CLR;
	LCD_RS_SET;

	for (i=0; i < ysize; i++) {
		for(j=0; j< xsize; j++) {
			diffmap[i][j]=1;
			drawmap[0][i][j]=0;
			LCD_WR_Data(0);
			drawmap[1][i][j]=255;
		}
	}

	flag=1;

	while (1) {

		numdiff=0;
		flag=1-flag;
		diffex=diffey=0;
		diffsx=diffsy=65535;
		//calculate the rectangle where changes occur
		for(i=0; i < ysize; i++){
			for(j=0; j < xsize; j++) {
				offset =  (i * xsize+ j)*2;
				p=(buffer[offset+1] << 8) | buffer[offset];

				//drawmap[flag][i>>1][j>>1] = p;
				if (drawmap[1-flag][i][j] != p) {
					drawmap[flag][i][j] = p;
					diffmap[i][j]=1;

					if (numdiff<MAXSINGLEPIXELS)
						{
							quickdiffmap[0][numdiff]=i;
							quickdiffmap[1][numdiff]=j;
						}
					drawmap[1-flag][i][j]=p;
					numdiff++;
					if ((i) < diffsx)
						diffsx = i;
					if ((i) > diffex)
						diffex = i ;
					if ((j)< diffsy)
						diffsy=j;
					if ((j)>diffey)
						diffey = j ;

				} else {
					diffmap[i][j]=0;
				}
				// offset++;
			}

		}
		/*if (numdiff > MAXSINGLEPIXELS){
		// printf ("(%d, %d) - (%d, %d)\n",diffsx, diffsy, diffex, diffey);

		area = ((abs(diffex - diffsx)+1)*(1+abs(diffey-diffsy)));
		//printf("diff:%d, area:%d, cov:%f\n",numdiff, area,(1.0*numdiff)/area);
		}
		*/
		//write individual pixels if there are less than 400 different ones
		if (numdiff < MAXSINGLEPIXELS){

			for (i=0;i<numdiff;i++)
				{
					write_dot(quickdiffmap[0][i],quickdiffmap[1][i],drawmap[flag][quickdiffmap[0][i]][quickdiffmap[1][i]]);
					//printf("quick: %i\n",numdiff);


				}
			//usleep(70000L);

			//fill complete rect
		} else{
			//define rect points
			LCD_WR_CMD(XS,diffsy);
			LCD_WR_CMD(YS,diffsx);
			LCD_WR_CMD(XE,diffey);
			LCD_WR_CMD(YE,diffex);
			//set initial pixel
			LCD_WR_CMD(XP,diffsy);
			LCD_WR_CMD(YP,diffsx);
			// LCD_WR_CMD( 0x003, 0x1238 );

			LCD_WR_REG(0x202);
			LCD_CS_CLR;
			LCD_RS_SET;
			//printf("slow: %i\n",numdiff);
			//printf ("(%d, %d) - (%d, %d)\n",diffsx, diffsy, diffex, diffey);
			for (i=diffsx; i<=diffex; i++){
				for (j=diffsy;j<=diffey; j++) {
					LCD_WR_Data(drawmap[flag][i][j]);
				}
			}
		}

		fseek(infile, 0, 0);

		if (fread (buffer, xsize * ysize *2, sizeof(unsigned char), infile) != 1)
			printf ("Read < %d chars when loading file %s\n", hsize*vsize*3, "ss");
		if (numdiff>0)
			if (numdiff>30000)
				isleep=50;
			else
				isleep=50000-(50000*numdiff/50000);
		else
			isleep=80000;

		//printf("numdiff: %i\t\tsleep %u\n",numdiff,isleep);
		usleep(isleep);

	}
}

void LCD_Init()
{
	LCD_RST_CLR;
	delay (100);
	LCD_RST_SET;
	delay (100);

	LCD_WR_CMD( 0x000, 0x0001 ); /* oschilliation start */
	delay ( 1 );
	/* Power settings */
	LCD_WR_CMD( 0x100, 0x0000 ); /*power supply setup*/
	LCD_WR_CMD( 0x101, 0x0000 );
	LCD_WR_CMD( 0x102, 0x3110 );
	LCD_WR_CMD( 0x103, 0xe200 );
	LCD_WR_CMD( 0x110, 0x009d );
	LCD_WR_CMD( 0x111, 0x0022 );
	LCD_WR_CMD( 0x100, 0x0120 );
	delay ( 2 );

	LCD_WR_CMD( 0x100, 0x3120 );
	delay ( 8 );
	/* Display control */

	LCD_WR_CMD( 0x002, 0x0000 );
#ifdef	ROTATE90
	LCD_WR_CMD( 0x001, 0x0000 );
	LCD_WR_CMD( 0x003, 0x12B8 );
#else
	LCD_WR_CMD( 0x001, 0x0100 );
	LCD_WR_CMD( 0x003, 0x1230 );
#endif
	LCD_WR_CMD( 0x006, 0x0000 );
	LCD_WR_CMD( 0x007, 0x0101 );
	LCD_WR_CMD( 0x008, 0x0808 );
	LCD_WR_CMD( 0x009, 0x0000 );
	LCD_WR_CMD( 0x00b, 0x0000 );
	LCD_WR_CMD( 0x00c, 0x0000 );
	LCD_WR_CMD( 0x00d, 0x0008 );
	/* LTPS control settings */
	LCD_WR_CMD( 0x012, 0x0000 );
	LCD_WR_CMD( 0x013, 0x0000 );
	LCD_WR_CMD( 0x018, 0x0000 );
	LCD_WR_CMD( 0x019, 0x0000 );

	LCD_WR_CMD( 0x203, 0x0000 );
	LCD_WR_CMD( 0x204, 0x0000 );

	LCD_WR_CMD( 0x210, 0x0000 );
	LCD_WR_CMD( 0x211, 0x00ef );
	LCD_WR_CMD( 0x212, 0x0000 );
	LCD_WR_CMD( 0x213, 0x013f );
	LCD_WR_CMD( 0x214, 0x0000 );
	LCD_WR_CMD( 0x215, 0x0000 );
	LCD_WR_CMD( 0x216, 0x0000 );
	LCD_WR_CMD( 0x217, 0x0000 );

	// Gray scale settings
	LCD_WR_CMD( 0x300, 0x5343);
	LCD_WR_CMD( 0x301, 0x1021);
	LCD_WR_CMD( 0x302, 0x0003);
	LCD_WR_CMD( 0x303, 0x0011);
	LCD_WR_CMD( 0x304, 0x050a);
	LCD_WR_CMD( 0x305, 0x4342);
	LCD_WR_CMD( 0x306, 0x1100);
	LCD_WR_CMD( 0x307, 0x0003);
	LCD_WR_CMD( 0x308, 0x1201);
	LCD_WR_CMD( 0x309, 0x050a);

	/* RAM access settings */
	LCD_WR_CMD( 0x400, 0x4027 );
	LCD_WR_CMD( 0x401, 0x0000 );
	LCD_WR_CMD( 0x402, 0x0000 ); /* First screen drive position (1) */
	LCD_WR_CMD( 0x403, 0x013f ); /* First screen drive position (2) */
	LCD_WR_CMD( 0x404, 0x0000 );

	LCD_WR_CMD( 0x200, 0x0000 );
	LCD_WR_CMD( 0x201, 0x0000 );

	LCD_WR_CMD( 0x100, 0x7120 );
	LCD_WR_CMD( 0x007, 0x0103 );
	delay( 1 );
	LCD_WR_CMD( 0x007, 0x0113 );
}

void LCD_test()
{
	int temp,num,i;
	char n;

	LCD_WR_CMD(XS,0x00);
	LCD_WR_CMD(YS,0x0000);
	LCD_WR_CMD(XE,MAX_X);
	LCD_WR_CMD(YE,MAX_Y);

	LCD_WR_CMD(XP,0x0000);
	LCD_WR_CMD(YP,0x0000);

	LCD_WR_REG(0x202);
	LCD_CS_CLR;
	LCD_RS_SET;
	for(n=0;n<8;n++)
		{
			temp=color[n];
			for(num=40*240;num>0;num--)
				{
					LCD_WR_Data(temp);
				}
		}
	for(n=0;n<8;n++)
		{
			LCD_WR_CMD(XS,0x00);
			LCD_WR_CMD(YS,0x0000);
			LCD_WR_CMD(XE,MAX_X);
			LCD_WR_CMD(YE,MAX_Y);

			LCD_WR_CMD(XP,0x0000);
			LCD_WR_CMD(YP,0x0000);

			LCD_WR_REG(0x202);
			LCD_CS_CLR;
			LCD_RS_SET;
			temp=color[n];
			for(i=0;i<240;i++)
				{
					for(num=0;num<320;num++)
						{
							LCD_WR_Data(temp);
						}
				}
		}
	LCD_CS_SET;
}

int main (int argc, char* argv[])
{
	printf("bcm2835 init now\n");
	if (!bcm2835_init())
		{
			printf("bcm2835 init error\n");
			return 1;
		}
	bcm2835_gpio_fsel(SPICS, BCM2835_GPIO_FSEL_OUTP) ;
	bcm2835_gpio_fsel(SPIRS, BCM2835_GPIO_FSEL_OUTP) ;
	bcm2835_gpio_fsel(SPIRST, BCM2835_GPIO_FSEL_OUTP) ;
	bcm2835_gpio_fsel(LCDPWM, BCM2835_GPIO_FSEL_OUTP) ;

#ifdef BCM2708SPI
	bcm2835_spi_begin();
	bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE3);
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_2);

	//bcm2835_spi_chipSelect(BCM2835_SPI_CS1);
	//bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS1,LOW);
#else
	bcm2835_gpio_fsel(SPISCL, BCM2835_GPIO_FSEL_OUTP) ;
	bcm2835_gpio_fsel(SPISCI, BCM2835_GPIO_FSEL_OUTP) ;
#endif
	LCD_PWM_CLR;
	printf ("Raspberry Pi MZTX06A LCD Testing...\n") ;
	printf ("http://jwlcd-tp.taobao.com\n") ;

	//  loadPGM("etao.pbm");
	//  for (;;)
	//{
	LCD_Init();
	if (argc>1)
		{
			if (strcmp(argv[1],"test")==0)
				{
					printf("test LCD\n");
					LCD_test();
				}
		}
	else
		loadFrameBuffer_diff_320();

	//LCD_showbuffer();
	//LCD_test();
	//LCD_clear(RGB565(130,130,150));
	//LCD_clear(0x00);
	//LCD_QQ();
	//draw_circle(100,100,50,RGB565(130,30,220));
	//draw_circle(200,10, 20,RGB565(100,2,23));
	//draw_circle(30,160, 80, RGB565(22,134,100));
	//DisplayString("Windows",1,0);
	//write_dot(10,10,0xffff);
	//delay(500);
	//y}
	return 0 ;
}

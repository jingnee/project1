/***********************************************************/
// ������ ����� �Լ��� ȣ���ϴ� ��ƾ�� ������ ����!
/***********************************************************/

#include "device_driver.h"
#define printf		Uart_Printf
#define main		User_Main

void User_Main(void);

void Main(void)
{
	MMU_Init();
	Uart_Init(115200);	
	printf("\n=================================\n");
	main();
	printf("=================================\n");
}

/*===== << ������� ����� ���α׷��� �ۼ��� >> =====*/

/***********************************************************/
// [1] : SD Access
/***********************************************************/

#if 0

/* 512B ������ ���͸� �μ����ִ� ������ �Լ� */

static void sector_printf(void * buf)
{
	int i, j, k;

	for(i=0; i<(128/8); i++)
	{
		printf("[%3d]", i*32);

		for(j=0; j<8; j++)
		{
			for(k=0; k<4; k++)
			{
				printf("%.2X", ((char *)buf)[i*32+j*4+k]);
			}
			printf(" ");
		}
		printf("\n");
	}
}

void main(void)
{
	int i, k;
	int buf[512/4];

	printf("SD Card Access Test\n");
	if(SD_Check_Card() != SD_SUCCESS) printf("SD Card�� ������ �ֽʽÿ�\n");

	for(;;)
	{
		if(SD_Check_Card() == SD_SUCCESS) break;
	}

	printf("SD_Check_Card() Passed!\n");

	i=SD_Init();
	printf("SD_Init() Passed!\n");
	if(i != SD_SUCCESS) printf("Init Err : [%d]\n", i);

	Key_Poll_Init();

	for(k=0;k<2;k++)
	{
		printf("�ƹ� KEY�� ������ �����մϴ�\n");

		Key_Wait_Key_Pressed();
		Key_Wait_Key_Released();

		SD_Read_Sector(k, 1, (U8 *)buf);
		printf("\n[%d]�� Sector Read!\n", k);
		sector_printf(buf);
	}
}

#endif

/***********************************************************/
// [2] : BMP Display �Լ�
/***********************************************************/

#if 0

void main(void)
{
	int add;
	Lcd_Graphic_Init();
	Lcd_Clr_Screen(0xf800);

	printf("24bpp BMP Display\n");

	printf("Address of BMP file, Default=0x31000000 => ");

	add = Uart_GetIntNum();

	if((add <= 0x30000000) || (add >= 0x32000000))
	{
		add = 0x31000000;
	}

	printf("%#x\n", add);
	Lcd_Draw_BMP_File_24bpp(0,0,(void *)add);
}

#endif

/***********************************************************/
// [3] : FAT16 ����
/***********************************************************/

#if 01

#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#pragma pack(push, 1)

/* MBR�� �𵨸��ϱ� ���� ����ü Ÿ�� ���� */
typedef struct{
	unsigned char rsvd[8];
	unsigned int LBA_Start;
	unsigned int Size;
}PARTITION;

typedef struct{
	unsigned char rsvd[446];
	PARTITION part[4];
	unsigned short signature;
}MBR;
/* BR�� �𵨸��ϱ� ���� ����ü Ÿ�� ���� */
typedef struct{
	unsigned char boot[11];
	unsigned short bytepersector;
	unsigned char sectorpercluster;
	unsigned short rsvdsectorcnt;
	unsigned char nooffats;
	unsigned short rootenterycnt;
	unsigned char rsvd1[3];
	unsigned short fat16size;
	unsigned char rsvd2[8];
	unsigned int totalsector;
}BR;

/* File Entry �м��� ���� ����ü, ����ü Ÿ�� ���� */
typedef struct{
	unsigned char r : 1;
	unsigned char h : 1;
	unsigned char s : 1;
	unsigned char v : 1;
	unsigned char d : 1;
	unsigned char a : 1;
}fat_file;
typedef struct{
	unsigned char ln : 4;
}long_file;
typedef union{
	fat_file n;
	long_file l;
	unsigned char c;
}FATATTR;
/* �ð� ������ ���� ��Ʈ�ʵ� ����ü ���� */
typedef struct{
	unsigned short sec : 5;
	unsigned short min : 6;
	unsigned short hour : 5;
}FATTIME;
/* ��¥ ������ ���� ��Ʈ�ʵ� ����ü ���� */
typedef struct{
	unsigned short day : 5;
	unsigned short mon : 4;
	unsigned short year : 7;
}FATDATE;
/* �ϳ��� 32B Entry �м��� ���� ����ü */
typedef struct
{/* Entry�� �� ����� �����Ѵ� */
	unsigned char name[11];
	FATATTR attr;
	unsigned char rsvd;
	unsigned char creationtimetenth;
	FATTIME creationtime;
	FATDATE creationdate;
	FATDATE lastaccessdate;
	unsigned short firstclusterhigh;
	FATTIME lastwritetime;
	FATDATE lastwritedate;
	unsigned short firstclusterlow;
	unsigned int filesize;
}ENTRY;

#pragma pack(pop)

/* MBR, BR �м��� ���Ͽ� ȹ���Ͽ� �����ؾ� �ϴ� ������ */

static struct _parameter
{
	unsigned int lba_start;
	unsigned short byte_per_sector;
	unsigned char sector_per_cluster;
	unsigned int root_sector_count;
	unsigned int fat0_start;
	unsigned int root_start;
	unsigned int file_start;
}parameter;

/* �Ϻ� ����Ǵ� �Լ� ��� */

//static void listing_file(int root_sector_cnt, int root_start, int byte_per_sector);
static void listing_file(void);
static void listing_delete_file(void);
static void listing_all_file(void);
static ENTRY * search_file(char *string);
static int check_file_type(ENTRY * file);
static void read_file(ENTRY * file, void * data);
static void sector_printf(void * buf);

/* 512B ������ ���͸� �о �����ϱ� ���� �ӽ� ���� */

static unsigned int buf[512/4];
/* 512B ������ ���͸� �μ����ִ� ������ �Լ� */
unsigned short fatSize;

static void sector_printf(void * buf)
{
	int i, j, k;

	for(i=0; i<(128/8); i++)
	{
		printf("[%3d]", i*32);

		for(j=0; j<8; j++)
		{
			for(k=0; k<4; k++)
			{
				printf("%.2X", ((char *)buf)[i*32+j*4+k]);
			}
			printf(" ");
		}
		printf("\n");
	}
}

void main(void)
{
	int i, curdir, rootdir;

	Lcd_Graphic_Init();
	Lcd_Clr_Screen(0xf800);

	if(SD_Check_Card() != SD_SUCCESS) printf("Insert SD Card Please!\n");

	for(;;)
	{
		if(SD_Check_Card() == SD_SUCCESS) break;
	}

	i=SD_Init();
	if(i != SD_SUCCESS) printf("Init Err : [%d]\n", i);

	/* ����� �ڵ� */

	{

#if 01
		SD_Read_Sector(0, 1, (U8 *)buf);

		/* MBR���� parameter.lba_start ���� �о �����Ѵ� */
		parameter.lba_start = ((MBR *)buf)->part[0].LBA_Start;

		printf("LBA_Start = %d\n", parameter.lba_start);

#endif

#if 01
		SD_Read_Sector(parameter.lba_start, 1, (U8 *)buf);

		/* BR���� �������� �о parameter ����ü ����鿡 �����Ѵ� */
		parameter.byte_per_sector = ((BR *)buf)->bytepersector;
		parameter.sector_per_cluster = ((BR *)buf)->sectorpercluster;
		parameter.fat0_start = parameter.lba_start + ((BR *)buf)->rsvdsectorcnt;
		parameter.root_start = parameter.fat0_start +
				((BR *)buf)->fat16size * ((BR *)buf)->nooffats;
		parameter.root_sector_count = ((BR *)buf)->rootenterycnt * 32 / parameter.byte_per_sector;
		parameter.file_start = parameter.root_start + parameter.root_sector_count;
		fatSize=((BR *)buf)->fat16size;

		listing_file();
#endif

		curdir = 0;
		rootdir = parameter.root_start;

		for(;;)
		{
			ENTRY * file;
			char *src, *dest;
			char cmd[10], str[11], nowname[11];
			int num, i;
			int r;

			printf("\n>> ");
			Uart_GetString(cmd);

			if(!strcmp(cmd,"DIR")){
				//listing_file(parameter.root_sector_count, parameter.root_start, parameter.byte_per_sector);
				listing_file();
			}

			else if(!strcmp(cmd,"DIR/A")){
				listing_all_file();
			}

			else if(!strcmp(cmd,"DIR/D")){
				listing_delete_file();
			}


			else if(!strcmp(cmd,"OPEN")){
				printf("��� ���ϸ��� �Է��Ͻÿ�.\n>");
				Uart_GetString(str);
				file = search_file(str);
				if((file == (ENTRY *)0) || file->attr.n.d == 1)
				{
					printf("��ġ�ϴ� ������ �����ϴ�.\n");
					continue;
				}

				for(i = 0; i < 8; i++) printf("%c", ((char *)file)[i]);
				printf(".");
				for(i = 8; i < (8+3); i++) printf("%c", ((char *)file)[i]);
				printf("\n");

			/* C, TXT, BMP ���� �Ǵ� ���͸����� Ȯ�� */

			r = check_file_type(file);
			//printf("File Type = %d\n", r);

			if(r != 0)
			{
				char * data;
				int size = file->filesize;

				/* ���� ũ�⸦ �ʰ��ϴ� ���� ������ �޸𸮷� �Ҵ��� �޾ƾ� �Ѵ� */
				num = size+parameter.byte_per_sector;
				//num = (size+parameter.byte_per_sector-1) & ~(parameter.byte_per_sector-1);
				data = malloc(num);
				
				/* ���� ������ �б� */

				read_file(file, data);

				switch(r)
				{
					case 1 :
					case 2 :
						for(i = 0; i < size; i++) printf("%c", ((char *)data)[i]);
						printf("\n");
						break;
					case 3 :
						Lcd_Clr_Screen(0xf800);
						Lcd_Draw_BMP_File_24bpp(0,0,(void *)data);
						break;
					default :
						break;
				}

				free(data);
			}

			else printf("�������� �ʴ� �����Դϴ�!\n");

		}

		else if(!strcmp(cmd,"CD.")){
			if(curdir == 0) strcpy(nowname,"ROOT");
			printf("������丮�� %s", nowname);
			printf(" ���丮 �Դϴ�.\n");
		}

		else if(!strcmp(cmd,"CD..")){
			if(curdir == 0) {
				printf("�ֻ��� ���丮 �Դϴ�!\n");
				continue;
			}

			SD_Read_Sector(parameter.root_start,1,(ENTRY *)buf);
			curdir = ((ENTRY *)buf)[1].firstclusterlow;

			if(curdir==0){
				SD_Read_Sector(parameter.lba_start, 1, (U8 *)buf);
				parameter.root_start = parameter.fat0_start +
								((BR *)buf)->fat16size * ((BR *)buf)->nooffats;
			}
			else parameter.root_start = parameter.file_start + parameter.sector_per_cluster*(curdir-2);

			listing_file();
		}

		else if(!strcmp(cmd,"CD")){
		printf("�̵��� ���丮 ���� �Է��Ͻÿ�\n>");
		Uart_GetString(str);
		file = search_file(str);
		strcpy(nowname,str);

		if(file->attr.n.d != 1 || file == (ENTRY *)0) {
			printf("��ġ�ϴ� ���丮�� �����ϴ�.\n");
			continue;
		}

		curdir=file->firstclusterlow;
		parameter.root_start= parameter.file_start+parameter.sector_per_cluster*(file->firstclusterlow-2);

		listing_file();
		}


		else if(!strcmp(cmd,"RENAME")){
			int ret;
			printf("������ ���ϸ�� ������ �̸��� �Է����ּ���.(Ȯ���ڱ��� �Է����ּ���.)\n>");
			Uart_GetString(str);
			src = strtok(str," ");
			dest = strtok(NULL, " ");
/*
			printf("%s���� ",src);
			printf("%s(��)�� ���� �մϴ�.\n",dest);
*/

			file = search_file(src);
			if(file == (ENTRY *)0 || file->attr.n.d == 1) {
				printf("���������ʴ� �����Դϴ�.\n");
				continue;
			}

			ret = rename_file(src,dest,0);
			if(ret == 0) printf("������ �̸��� �����մϴ�!\n");
			else if(ret == 2)printf("�ùٸ� ���ϸ��� �ƴմϴ�. Ȯ���ڸ� �Է����ּ���!\n");
			//else printf("����!\n");

		}

		else if(!strcmp(cmd,"DEL")){
			int ret;
			printf("���� ���ϸ��� �Է����ּ���.\n>");
			Uart_GetString(str);

			file = search_file(str);
			if(file == (ENTRY *)0 || file->attr.n.d == 1) {
			printf("���������ʴ� �����Դϴ�.\n");
				continue;
						}
			//Entry����
			ret = rename_file(str,".",1);

			//if(ret==1)printf("����?\n");

			delete_file(file);

		}

		else if(!strcmp(cmd,"EXIT")) break;

		else
			printf("��ɾ �ùٸ��� �ʽ��ϴ�!\n");

		}
		}
		printf("�ȳ�~!\n");
	}

static void entryprint(int num, ENTRY *buf){
	int i;
	printf("[%3d] ", num);
	for (i = 0; i < 8; i++) printf("%c", buf->name[i]);
	printf(".");
	for (i = 8; i < 11; i++) printf("%c", buf->name[i]);
	printf(" 0x%x ", buf->attr.c);
	printf("%02d/%02d/%02d ", buf->creationdate.year+1980,
			buf->creationdate.mon, buf->creationdate.day);
	printf("%02d:%02d:%02d ", buf->creationtime.hour,
			buf->creationtime.min, buf->creationtime.sec*2);
	printf("%5d %10d\n", buf->firstclusterlow, buf->filesize);
}
//static void listing_file(int root_sector_cnt, int root_start, int byte_per_sector){
static void listing_file(){
	int i, j, num = 1;
	/* Ÿ��Ʋ �μ� => ���� ���� ���� */
	printf("[NUM] [NAME   .EXT] [AT] [  DATE  ] [TIME] [CLUST] [ SIZE ]\n");
	printf("=============================================================\n");

	for(i = 0; i < parameter.root_sector_count; i++)
	{
		SD_Read_Sector(parameter.root_start + i, 1, (U8 *)buf);

		for(j = 0; j < (parameter.byte_per_sector / 32); j++)
		{
			/* Name[0]�� 0x0�̸� �μ� ����, 0x05, 0xE5�̸� �������� Skip */
			if (((ENTRY*)buf)[j].name[0] == 0x00) goto EXIT;//�μ� ����
			if (((ENTRY*)buf)[j].name[0] == 0x05) continue;//�������� Skip
			if (((ENTRY*)buf)[j].name[0] == 0xE5) continue;//�������� Skip
			/* ���� �Ӽ��� 0x3F �Ǵ� 0x0F long file name �̹Ƿ� Skip */
			if (((ENTRY*)buf)[j].attr.l.ln == 0xf) continue;
			/* Entry ���� �μ� */
			entryprint(num, &((ENTRY *)buf)[j]);
			/* �μ�Ǵ� ���� �Ǵ� ���� ���� �� �տ� 1������ 1�� �����ϸ� ��ȣ�� �μ��Ѵ� */
			num++;
		}
	}
EXIT:
	printf("=============================================================\n");
}

static void listing_delete_file(void){
	int i, j, num = 1;
		/* Ÿ��Ʋ �μ� => ���� ���� ���� */
		printf("[NUM] [NAME   .EXT] [AT] [  DATE  ] [TIME] [CLUST] [ SIZE ]\n");
		printf("=============================================================\n");

		for(i = 0; i < parameter.root_sector_count; i++)
		{
			SD_Read_Sector(parameter.root_start + i, 1, (U8 *)buf);

			for(j = 0; j < (parameter.byte_per_sector / 32); j++)
			{
				/* Name[0]�� 0x0�̸� �μ� ����, 0x05, 0xE5�̸� �������� Skip */
				if (((ENTRY*)buf)[j].name[0] == 0x00) goto EXIT;//�μ� ����
				/* ���� �Ӽ��� 0x3F �Ǵ� 0x0F long file name �̹Ƿ� Skip */
				if (((ENTRY*)buf)[j].attr.l.ln == 0xf) continue;
				if (((ENTRY*)buf)[j].name[0] == 0x05 || ((ENTRY*)buf)[j].name[0] == 0xE5) {
				/* Entry ���� �μ� */
				entryprint(num, &((ENTRY *)buf)[j]);
				/* �μ�Ǵ� ���� �Ǵ� ���� ���� �� �տ� 1������ 1�� �����ϸ� ��ȣ�� �μ��Ѵ� */
				num++;
				}
			}
		}
	EXIT:
		printf("=============================================================\n");
}

static void listing_all_file(void){
	int i, j, num = 1;
		/* Ÿ��Ʋ �μ� => ���� ���� ���� */
		printf("[NUM] [NAME   .EXT] [AT] [  DATE  ] [TIME] [CLUST] [ SIZE ]\n");
		printf("=============================================================\n");

		for(i = 0; i < parameter.root_sector_count; i++)
		{
			SD_Read_Sector(parameter.root_start + i, 1, (U8 *)buf);

			for(j = 0; j < (parameter.byte_per_sector / 32); j++)
			{
				/* Name[0]�� 0x0�̸� �μ� ����, 0x05, 0xE5�̸� �������� Skip */
				if (((ENTRY*)buf)[j].name[0] == 0x00) goto EXIT;//�μ� ����
				/* ���� �Ӽ��� 0x3F �Ǵ� 0x0F long file name �̹Ƿ� Skip */
				if (((ENTRY*)buf)[j].attr.l.ln == 0xf) continue;

				/* Entry ���� �μ� */
				entryprint(num, &((ENTRY *)buf)[j]);
				/* �μ�Ǵ� ���� �Ǵ� ���� ���� �� �տ� 1������ 1�� �����ϸ� ��ȣ�� �μ��Ѵ� */
				num++;
			}
		}
	EXIT:
		printf("=============================================================\n");
}

#if 1

static ENTRY *search_file(char *string){
	int i = 0, j, nameSize=0, extSize=0;

	while (string[i++] != '.') nameSize++;
	while (string[i++] != '\0')extSize++;

	for(i = 0; i < parameter.root_sector_count; i++)
		{
			SD_Read_Sector(parameter.root_start + i, 1, (U8 *)buf);

			for(j = 0; j < (parameter.byte_per_sector / 32); j++)
			{
				/* Name[0]�� 0x0�̸� NULL ������ ����, 0x05, 0xE5�̸� Skip */
				if (((ENTRY*)buf)[j].name[0] == 0x00) return (ENTRY *)0;//�μ� ����
				if (((ENTRY*)buf)[j].name[0] == 0x05) continue;//�������� Skip
				if (((ENTRY*)buf)[j].name[0] == 0xE5) continue;//�������� Skip
				/* ���� �Ӽ��� 0x3F �Ǵ� 0x0F long file name �̹Ƿ� Skip */
				if (((ENTRY*)buf)[j].attr.l.ln == 0xf) continue;
				/* ���� Entry(����, ����)�� ��� num�� ���ϴ� ID���� Ȯ�� */
				/* num�� id���� ������ num ������ �ϰ� ��� Ž�� �ݺ�, ������ Entry ���� �ּ� ���� */
				if (strncmp(string, ((ENTRY*)buf)[j].name,nameSize)==0 && strncmp(string+nameSize+1, ((ENTRY*)buf)[j].name+8,extSize)==0) return &((ENTRY*)buf)[j];

				//���͸� �ϰ��
				if(((ENTRY*)buf)[j].attr.n.d == 1 && strncmp(string, ((ENTRY*)buf)[j].name,strlen(string))==0) return &((ENTRY*)buf)[j];
			}
		}

		return (ENTRY *)0;
}


#endif

static int check_file_type(ENTRY * file)
{
	static char * cmp[] = {"C  ", "TXT", "BMP"};
	int i;
	/* ���� �� : C ���� => 1, TXT ���� => 2, BMP ���� => 3 �׿� => 0 ���� */
	if (file->attr.n.a == 0) return 0;//file �ƴ�
	for (i = 8; i < 11; i++) {
		if (('a' <= file->name[i]) && (file->name[i] <= 'z')){
			file->name[i] = file->name[i] - 'a' + 'A';//�ҹ��ڸ� �빮�ڷ� ����
		}
	}
	for (i = 0; i < 3; i++){
		if (strncmp((char *)&file->name[8], cmp[i], 3) == 0) return i+1;
	}
	return 0;
}

static void read_file(ENTRY * file, void * data)
{
	/* �־��� Entry�� ���� �����͸� �о data �ּҿ� �����Ѵ� */
	U16 *fatbuf = malloc(parameter.byte_per_sector);
		int prefatsecoffset = -1;//������ ���� fat ���̺� offset(�ʱⰪ �ü����� ��, ���ʴ� �о�� �ϹǷ�)
		int curfatsecoffset;//���� �о�� �ϴ� fat ���̺� offset
		int nextclusternum = file->firstclusterlow;//������ ���� cluster ��ȣ
		int curclusternum;//���� �о���� cluster ��ȣ
		int addrsec;
		int size = file->filesize;//�о���� ���� ũ��
		U8 *databuf = data;//������ �ּ�
		int i, cnt;
		while (nextclusternum != 0xFFFF){
			curclusternum = nextclusternum;
			curfatsecoffset = curclusternum / (parameter.byte_per_sector/2);//���� Ŭ������ ��ȣ�� �ش��ϴ� ����
			if (prefatsecoffset != curfatsecoffset){//�ٸ� ���͸� �о�� ��
				prefatsecoffset = curfatsecoffset;
				SD_Read_Sector(parameter.fat0_start + curfatsecoffset, 1, (U8*)fatbuf);
			}
			nextclusternum = fatbuf[curclusternum%(parameter.byte_per_sector/2)];
			if (nextclusternum == 0xFFFF){//������ cluster �� �ܷ��� �б�
				cnt = (size + parameter.byte_per_sector - 1) / parameter.byte_per_sector;
			}
			else{
				cnt = parameter.sector_per_cluster;
			}
			addrsec = parameter.file_start + (parameter.sector_per_cluster * (curclusternum-2));
			for (i = 0; i < cnt; i++){
				SD_Read_Sector(addrsec+i, 1, databuf);
				databuf += parameter.byte_per_sector;//�ּ� ����
				size -= parameter.byte_per_sector;//ũ�� ����
			}
		}
		free(fatbuf);
}

int rename_file(char *src, char *dest, int del){
	char name[12];
	int i = 0, j = 0, k, flag = 0, nameSize=0, extSize=0;

	if(search_file(dest) != (ENTRY *)0 && del == 0) return 0;

	while(1){
		if(dest[j]=='\0'){
			while(i<11)name[i++]=' ';
			break;
		}
		if(dest[j]=='.'){
			flag=1;
			j++;
			while(i<8)name[i++]=' ';
		}
		name[i++]=dest[j++];
	}
	name[11]='\0';

	if(flag==0){
		return 2;
	}

	nameSize=0, extSize=0, i=0;
	while (src[i++] != '.') nameSize++;
	while (src[i++] != '\0')extSize++;

	for(i = 0; i < parameter.root_sector_count; i++)
		{
			SD_Read_Sector(parameter.root_start + i, 1, (U8 *)buf);

			for(j = 0; j < (parameter.byte_per_sector / 32); j++)
			{
				/* num�� id���� ������ num ������ �ϰ� ��� Ž�� �ݺ�, ������ Entry ���� �ּ� ���� */
				if (strncmp(src, ((ENTRY*)buf)[j].name,nameSize)==0 && strncmp(src+nameSize+1, ((ENTRY*)buf)[j].name+8,extSize)==0){
					if(del==0) {
						for(k=0;k<11;k++)((ENTRY*)buf)[j].name[k]=name[k];
					}
					else ((ENTRY*)buf)[j].name[0]=0xE5;

					SD_Write_Sector(parameter.root_start+i,1,(U8*)buf);
					return 1;
				}
				}

			}
	return 0;
		}

#endif

#if 1
void delete_file(ENTRY * file)
{
	U16 *fatbuf = malloc(parameter.byte_per_sector);
	int prefatsecoffset = -1;//������ ���� fat ���̺� offset(�ʱⰪ �ü����� ��, ���ʴ� �о�� �ϹǷ�)
	int curfatsecoffset;//���� �о�� �ϴ� fat ���̺� offset
	int nextclusternum = file->firstclusterlow;//������ ���� cluster ��ȣ
	int curclusternum;//���� �о���� cluster ��ȣ
	unsigned int fat1_start;


	fat1_start = parameter.fat0_start+fatSize;

	while (nextclusternum != 0xFFFF){
		curclusternum = nextclusternum;
		curfatsecoffset = curclusternum / (parameter.byte_per_sector/2);//���� Ŭ������ ��ȣ�� �ش��ϴ� ����
		printf("curclusternum %d\n", curclusternum);
		if (prefatsecoffset != curfatsecoffset){//�ٸ� ���͸� �о�� ��
			prefatsecoffset = curfatsecoffset;
			SD_Read_Sector(parameter.fat0_start + curfatsecoffset, 1, (U8*)fatbuf);
		}
		nextclusternum = fatbuf[curclusternum%(parameter.byte_per_sector/2)];
		fatbuf[curclusternum%(parameter.byte_per_sector/2)]=0x00;

		SD_Write_Sector(parameter.fat0_start+curfatsecoffset,1,(U8*)fatbuf);
		SD_Write_Sector(fat1_start+curfatsecoffset,1,(U8*)fatbuf);
	}
	//nextclusternum�� 0XFFFF�϶�
	//SD_Write_Sector(parameter.fat0_start+curfatsecoffset,1,(U8*)fatbuf);
	//SD_Write_Sector(fat1_start+curfatsecoffset,1,(U8*)fatbuf);

	free(fatbuf);
}

#endif

#if 0
void renamefunc(ENTRY *file, int curdir){
	int num=0, ixd=0;

	char cmd[50];
	char str1[50];
	char str2[50];
	printf("������ ���ϸ� Ȥ�� �������� �Է��Ͻÿ�!\n> ");
	Uart_GetString(cmd);
	convstr(str1, cmd);
	idx = search_file(str1);
	if(idx == -1){
		printf("���� ���ϸ� Ȥ�� ������ �Դϴ�!\n");
		return ;
	}
	printf("���ο� ���ϸ� Ȥ�� �������� �Է��Ͻÿ�!\n> ");
	Uart_GetString(cmd);
	convstr(str2, cmd);
	num = search_fild(str2);
	if(num==idx){
		printf("���� ���ϸ� Ȥ�� ������� �����մϴ�!\n");
		return;
	}
	if(num!=-1){
		printf("�����ϴ� ���ϸ� Ȥ�� ������ �Դϴ�!\n");
		return;
	}
	strncpy((char*)file[idx].name,str2,11);
}
#endif


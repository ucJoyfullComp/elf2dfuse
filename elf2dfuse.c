/*
    command-line tool to convert a STM32 ELF object file into a ST DfuSe image
    Copyright (C) 2017 Peter Lawrence

    Permission is hereby granted, free of charge, to any person obtaining a 
    copy of this software and associated documentation files (the "Software"), 
    to deal in the Software without restriction, including without limitation 
    the rights to use, copy, modify, merge, publish, distribute, sublicense, 
    and/or sell copies of the Software, and to permit persons to whom the 
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in 
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
    DEALINGS IN THE SOFTWARE.
*/

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>


#define USB_VENDOR_ID  0x0483
#define USB_PRODUCT_ID 0xdf11

typedef struct Elf32_Ehdr
{
	uint8_t e_ident[16];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint32_t e_entry;
	uint32_t e_phoff;
	uint32_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
} Elf32_Ehdr;

typedef struct Elf32_Shdr
{
	uint32_t sh_name;
	uint32_t sh_type;
	uint32_t sh_flags;
	uint32_t sh_addr;
	uint32_t sh_offset;
	uint32_t sh_size;
	uint32_t sh_link;
	uint32_t sh_info;
	uint32_t sh_addralign;
	uint32_t sh_entsize;
} Elf32_Shdr;

typedef struct Elf32_Phdr
{
	uint32_t p_type;
	uint32_t p_offset;
	uint32_t p_vaddr;
	uint32_t p_paddr;
	uint32_t p_filesz;
	uint32_t p_memsz;
	uint32_t p_flags;
	uint32_t p_align;
} Elf32_Phdr;

struct memory_blob
{
	uint32_t address, count;
	uint8_t *data;
	struct memory_blob *next;
};

typedef struct _vidpid_t {
    unsigned short int vid;
    unsigned short int pid;
    struct _vidpid_t *next;
} vidpid_t;

static Elf32_Ehdr eh; 

static uint8_t read_uint8(FILE *fp)
{
	uint8_t c;
	c = fgetc(fp);
	return c; 
}

static uint16_t read_uint16(FILE *fp)
{
	uint16_t c;
	c = fgetc(fp);
	c |= fgetc(fp)<<8;
	return c; 
}

static uint32_t read_uint32(FILE *fp)
{
	uint32_t c;
	c = fgetc(fp);
	c |= fgetc(fp)<<8;
	c |= fgetc(fp)<<16;
	c |= fgetc(fp)<<24;
	return c; 
}

static void printEh(void)
{
	int i;
	printf("e_ident[0] %x\n",eh.e_ident[0]);
	printf("e_ident[1-3] ");
	for(i=1;i<4;i++)
	{
		printf("%c",eh.e_ident[i]);
	}
	printf("\n");
	printf("e_ident[4] %x\n",eh.e_ident[4]);
	printf("e_ident[5] %x\n",eh.e_ident[5]);
	printf("e_ident[6] %x\n",eh.e_ident[6]);
	printf("e_ident[7] %x\n",eh.e_ident[7]);
	printf("e_type %x\n",eh.e_type);
	printf("e_machine %x\n",eh.e_machine);
	printf("e_version %x\n",eh.e_version);
	printf("e_entry %x\n",eh.e_entry);
	printf("e_phoff %x\n",eh.e_phoff);
	printf("e_shoff %x\n",eh.e_shoff);
	printf("e_flags %x\n",eh.e_flags);
	printf("e_ehsize %x\n",eh.e_ehsize);
	printf("e_phentsize %x\n",eh.e_phentsize);
	printf("e_phnum %x\n",eh.e_phnum);
	printf("e_shentsize %x\n",eh.e_shentsize);
	printf("e_shnum %x\n",eh.e_shnum);
	printf("e_shstrndx %x\n",eh.e_shstrndx);
}

void readEh(FILE *fp)
{
	int i;
	for(i=0;i<16;i++)
	{
		eh.e_ident[i]=read_uint8(fp);
	}
	eh.e_type = read_uint16(fp);
	eh.e_machine = read_uint16(fp);
	eh.e_version = read_uint32(fp);
	eh.e_entry = read_uint32(fp);
	eh.e_phoff = read_uint32(fp);
	eh.e_shoff = read_uint32(fp);
	eh.e_flags = read_uint32(fp);
	eh.e_ehsize = read_uint16(fp);
	eh.e_phentsize = read_uint16(fp);
	eh.e_phnum = read_uint16(fp);
	eh.e_shentsize = read_uint16(fp);
	eh.e_shnum = read_uint16(fp);
	eh.e_shstrndx = read_uint16(fp);
}

static int checkEh(void)
{
	int error = 0;
	const uint8_t ref_ident[7] = { 0x7F, 'E', 'L', 'F', 1, 1, 1 };

	if (memcmp(eh.e_ident, ref_ident, sizeof(ref_ident)))
	{
		error = 1;
		printf("ERROR: wrong e_ident\n");
	}
	if (eh.e_type != 2)
	{
		error = 1;
		printf("ERROR: wrong e_type 0x%x != 0x2\n",eh.e_type);
	}
	if (eh.e_machine != 40)
	{
		error = 1;
		printf("ERROR: wrong e_machine 0x%x != 0x28\n",eh.e_machine);
	}
	if (eh.e_version != 1)
	{
		error = 1;
		printf("ERROR: wrong e_version 0x%x != 0x1\n",eh.e_version);
	}
	return error;
}

static Elf32_Shdr readSh(int num,FILE *fp)
{
	Elf32_Shdr sh;

	fseek(fp,num*eh.e_shentsize+eh.e_shoff,SEEK_SET);
	sh.sh_name = read_uint32(fp);
	sh.sh_type = read_uint32(fp);
	sh.sh_flags = read_uint32(fp);
	sh.sh_addr = read_uint32(fp);
	sh.sh_offset = read_uint32(fp);
	sh.sh_size = read_uint32(fp);
	sh.sh_link = read_uint32(fp);
	sh.sh_info = read_uint32(fp);
	sh.sh_addralign = read_uint32(fp);
	sh.sh_entsize = read_uint32(fp);

	return sh;
}

static Elf32_Phdr readPh(int num,FILE *fp)
{
	Elf32_Phdr ph;

	fseek(fp,num*eh.e_phentsize+eh.e_phoff,SEEK_SET);
	ph.p_type = read_uint32(fp);
	ph.p_offset = read_uint32(fp);
	ph.p_vaddr = read_uint32(fp);
	ph.p_paddr = read_uint32(fp);
	ph.p_filesz = read_uint32(fp);
	ph.p_memsz = read_uint32(fp);
	ph.p_flags = read_uint32(fp);
	ph.p_align = read_uint32(fp);

	return ph;
}

static struct memory_blob *find_blob(uint32_t address, uint32_t count, struct memory_blob **list)
{
	struct memory_blob *current, *previous, *addition;

	current = *list; previous = NULL;
	while (current)
	{
		if (current->address > address)
			break;

		previous = current;
		current = current->next;
	}

	addition = malloc(sizeof(struct memory_blob));
	memset(addition, 0, sizeof(struct memory_blob));

	addition->data = malloc(count);
	addition->address = address;
	addition->count = count;
	addition->next = current;

	if (previous)
		previous->next = addition;
	else
		*list = addition;

	return addition;
}

static const uint32_t crc32_table[256] =
{
	0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
	0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
	0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
	0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
	0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
	0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
	0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
	0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
	0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
	0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
	0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
	0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
	0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
	0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
	0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
	0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
	0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
	0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
	0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
	0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
	0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
	0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
	0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
	0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
	0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
	0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
	0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
	0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
	0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
	0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
	0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
	0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
	0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
	0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
	0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
	0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
	0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
	0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
	0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
	0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
	0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
	0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
	0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
	0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
	0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
	0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
	0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
	0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
	0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
	0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
	0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
	0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
	0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
	0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
	0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
	0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
	0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
	0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
	0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
	0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
	0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
	0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
	0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
	0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D,
};

static uint32_t crc32_calc(uint32_t crc, uint8_t *buffer, uint32_t length)
{
	while (length--)
		crc = crc32_table[(crc ^ *buffer++) & 0xff] ^ (crc >> 8);

	return crc;
}

static vidpid_t *readvidPidList(FILE *fp)
{
    vidpid_t *head, *p, *tp;
    unsigned int vid, pid;

    p = tp = head = (vidpid_t *)NULL;
    if (fp) {
        vid = pid = 0;
        while (fscanf(fp, "%x:%x\n", &vid, &pid) == 2)
        {
            if ((vid == 0) || (vid > 0xFFFF) || (pid > 0xFFFF)) 
            {
                printf("ERROR: vidpid file internal format error.\n");
                exit(-1);
            }
            tp = (vidpid_t *)malloc(sizeof(vidpid_t));
            tp->vid = vid;
            tp->pid = pid;
            tp->next = (struct _vidpid_t *)NULL;
            if (p == (vidpid_t *)NULL) 
                p = tp;
            else
                p->next = (struct _vidpid_t *)tp;
            if (head == (vidpid_t *)NULL) head = p;
            p = tp;
            tp = (vidpid_t *)NULL;
        }
        return head;
    }
    return (vidpid_t *)NULL;
}

static void freevidPidList(vidpid_t *head)
{
    vidpid_t *p;
    
    if (head == (vidpid_t *)NULL) return;
    while (head) {
        p = (vidpid_t *)head->next;
        free(head);
        head = p;
    }
}

static vidpid_t *removeFromListHead(vidpid_t **head)
{
    vidpid_t *ret;

    if (head == (vidpid_t **)NULL) return (vidpid_t *)NULL;
    if (*head == (vidpid_t *)NULL) return *head;
    ret = *head;
    *head = (vidpid_t *)ret->next;
    ret->next = (struct _vidpid_t *)NULL;
    return ret;
}

off_t fsize(const char *filename) {
    struct stat st;

    if (stat(filename, &st) == 0)
        return st.st_size;

    fprintf(stderr, "Cannot determine size of %s: %s\n",
            filename, strerror(errno));

    return -1;
}

FILE *elffp = (FILE *)NULL;
FILE *dfufp = (FILE *)NULL;
FILE *vidpidfp = (FILE *)NULL; /* file containing a list of vid:pid pairs for */
                               /* vendor product specific DFU files */

void closeOpenFiles()
{
    if (vidpidfp != ((FILE *)NULL)) {
        fclose(vidpidfp);
    }
	if (elffp != ((FILE *)NULL)) {
        fclose(elffp);
    }
	if (dfufp != ((FILE *)NULL)) {
        fclose(elffp);
    }
}

Elf32_Phdr *ph = (Elf32_Phdr *)NULL;
vidpid_t *head = (vidpid_t *)NULL;
struct memory_blob *blob = (struct memory_blob *)NULL;
struct memory_blob *pm_list = (struct memory_blob *)NULL;
struct memory_blob *seek = (struct memory_blob *)NULL;
uint8_t *filebuf = (uint8_t *)NULL;
uint8_t *fname = (uint8_t *)NULL;
uint8_t *fext = (uint8_t *)NULL;
uint8_t *fullname = (uint8_t *)NULL;

void freeAllAllocatedMemory()
{
    if (ph != (Elf32_Phdr *)NULL)
    {
        free(ph);
        ph = (Elf32_Phdr *)NULL;
    }
    if (head != (vidpid_t *)NULL)
    {
        freevidPidList(head);
        head = (vidpid_t *)NULL;
    }
    if (blob != (struct memory_blob *)NULL)
    {
        while (blob != (struct memory_blob *)NULL)
        {
            seek = blob->next;
            free(blob);
            blob = seek;
        }
    }
    if (filebuf != (uint8_t *)NULL)
    {
        free(filebuf);
            if (vidpidfp != ((FILE *)NULL)) {
        fclose(vidpidfp);
    }
	if (elffp != ((FILE *)NULL)) {
        fclose(elffp);
    }
	if (dfufp != ((FILE *)NULL)) {
        fclose(elffp);
    }
}

Elf32_Phdr *ph = (Elf32_Phdr *)NULL;
vidpid_t *head = (vidpid_t *)NULL;
struct memory_blob *blob = (struct memory_blob *)NULL;
struct memory_blob *pm_list = (struct memory_blob *)NULL;
struct memory_blob *seek = (struct memory_blob *)NULL;
uint8_t *filebuf = (uint8_t *)NULL;
uint8_t *fname = (uint8_t *)NULL;
uint8_t *fext = (uint8_t *)NULL;
uint8_t *fullname = (uint8_t *)NULL;

void freeAllAllocatedMemory()
{
filebuf = (uint8_t *)NULL;
    }
    if (fname != (uint8_t *)NULL)
    {
        free(fname);
        fname = (uint8_t *)NULL;
    }
    if (fext != (uint8_t *)NULL)
    {
        free(fext);
        fext = (uint8_t *)NULL;
    }
    if (fullname != (uint8_t *)NULL)
    {
        free(fullname);
        fullname = (uint8_t *)NULL;
    }
}

int main(int argc, char *argv[])
{
	int i, j;
	Elf32_Shdr sh;
	uint32_t phy_addr, image_elements, file_size, crc32, target_size, element_size;
    uint32_t crc32_clean;
	uint8_t scratchpad[274 /* sized specifically for DfuSe Target Prefix */];
    off_t filesize, bwritten;
    vidpid_t *current_vidpid;
    char *cp = (char *)NULL;
    long int lenlen;

    atexit(freeAllAllocatedMemory);
    atexit(closeOpenFiles);

	if (argc < 4)
	{
		printf("%s <input.elf> <output[_vid_pid].dfu> <vidpid.txt>\n", argv[0]);
		return -1;
	}

	elffp = fopen(argv[1], "rb");
	if (!elffp)
	{
		printf("ERROR: unable to open file <%s> for reading\n", argv[1]);
		return -1;
	}
	dfufp = fopen(argv[2], "wb");
	if (!dfufp)
	{
		printf("ERROR: unable to open file <%s> for writing\n", argv[2]);
		return -1;
	}
    vidpidfp = fopen(argv[3], "rt");
	if (!vidpidfp)
	{
		printf("ERROR: unable to open test file <%s> for reading\n", argv[3]);
		return -1;
	}


    /*
    read and parse vid:PID list
    */
    head = (vidpid_t *)NULL;
    head = readvidPidList(vidpidfp);
    if (head == (vidpid_t *)NULL)
    {
        printf("ERROR: format of file %s is incorrect or file is empty "
               "or non existent\n", argv[3]);
        return -1;
    }
    fclose(vidpidfp);
    vidpidfp = (FILE *)NULL;

	/*
	read (and check) ELF header
	*/

	readEh(elffp); 
	if (checkEh()) 
	{
		printEh();
		return -1;
	}

	/*
	read ELF Program Headers
	*/

	ph = (Elf32_Phdr *)malloc(eh.e_phnum * sizeof(Elf32_Phdr));
	for(i=0;i<eh.e_phnum;i++)
	{
		ph[i] = readPh(i,elffp);
	}

	/*
	read ELF Section Headers, and add relevant sections (re-mapped to physical address) to blob list
	*/

	pm_list = NULL;
	for(i=0;i<eh.e_shnum;i++)
	{
		sh = readSh(i,elffp);

		if ( (sh.sh_type==1) && (sh.sh_flags & 0x6) )
		{
			for (j = 0; j < eh.e_phnum; j++)
			{
				if ( (sh.sh_addr >= ph[j].p_vaddr) && (sh.sh_addr < (ph[j].p_vaddr + ph[j].p_memsz)) && ph[j].p_filesz )
				{
					phy_addr = ph[j].p_paddr + (sh.sh_addr - ph[j].p_vaddr);
					blob = find_blob(phy_addr, sh.sh_size, &pm_list);

					fseek(elffp, sh.sh_offset, SEEK_SET);
					fread(blob->data, blob->count, 1, elffp);
					break;
				}
			}
		} 
	}

	/* we've read everything we need from the ELF file, so we can close the file */
	fclose(elffp);
    elffp = (FILE *)NULL;

	/*
	count the number of non-contiguous memory "image_elements" as well as tabulate the total file size
	*/

	blob = pm_list; image_elements = 1; i = 0;
	
	file_size = 11 /* DfuSe prefix */ + sizeof(scratchpad) /* Target Prefix */ + 16 /* DFU Suffix */; target_size = 0;

	while (blob)
	{
		phy_addr = blob->address;
		file_size += 8 /* Image Element */; target_size += 8 /* Image Element */;

		while (blob)
		{
			if (phy_addr != blob->address)
			{
				image_elements++;
				break;
			}

			phy_addr += blob->count; file_size += blob->count; target_size += blob->count;

			blob = blob->next;
		}
	}

	/*
	write unique-to-ST DfuSe 'DFU Prefix'
	*/

	i = 0;
	scratchpad[i++] = 'D';
	scratchpad[i++] = 'f';
	scratchpad[i++] = 'u';
	scratchpad[i++] = 'S';
	scratchpad[i++] = 'e';
	scratchpad[i++] = 0x01 /* bVersion */;
	scratchpad[i++] = (uint8_t)(file_size >> 0);
	scratchpad[i++] = (uint8_t)(file_size >> 8);
	scratchpad[i++] = (uint8_t)(file_size >> 16);
	scratchpad[i++] = (uint8_t)(file_size >> 24);
	scratchpad[i++] = 1 /* bTargets */;

	crc32 = crc32_calc(0xFFFFFFFF, scratchpad, i);
	fwrite(scratchpad, i, 1, dfufp);

	/*
	write unique-to-ST DfuSe 'Target Prefix' (first part of 'DFU Image')
	*/

	memset(scratchpad, 0, sizeof(scratchpad));
	scratchpad[0] = 'T';
	scratchpad[1] = 'a';
	scratchpad[2] = 'r';
	scratchpad[3] = 'g';
	scratchpad[4] = 'e';
	scratchpad[5] = 't';
	scratchpad[266] = (uint8_t)(target_size >> 0);
	scratchpad[267] = (uint8_t)(target_size >> 8);
	scratchpad[268] = (uint8_t)(target_size >> 16);
	scratchpad[269] = (uint8_t)(target_size >> 24);
	scratchpad[270] = (uint8_t)(image_elements >> 0);
	scratchpad[271] = (uint8_t)(image_elements >> 8);
	scratchpad[272] = (uint8_t)(image_elements >> 16);
	scratchpad[273] = (uint8_t)(image_elements >> 24);

	crc32 = crc32_calc(crc32, scratchpad, sizeof(scratchpad));
	fwrite(scratchpad, sizeof(scratchpad), 1, dfufp);

	/*
	write blob list to raw file
	*/

	blob = pm_list;

	while (blob)
	{
		/*
		seek ahead to tabulate the total size of this "element" (section of contiguous memory)
		*/

		phy_addr = blob->address; element_size = 0;

		seek = blob;

		while (seek)
		{
			if (phy_addr != seek->address)
				break;

			phy_addr += seek->count; element_size += seek->count;

			seek = seek->next;
		}
	
		/*
		write unique-to-ST DfuSe 'Image Element' prefix (providing origin address and size)
		*/

		i = 0;
		scratchpad[i++] = (uint8_t)(blob->address >> 0);
		scratchpad[i++] = (uint8_t)(blob->address >> 8);
		scratchpad[i++] = (uint8_t)(blob->address >> 16);
		scratchpad[i++] = (uint8_t)(blob->address >> 24);
		scratchpad[i++] = (uint8_t)(element_size >> 0);
		scratchpad[i++] = (uint8_t)(element_size >> 8);
		scratchpad[i++] = (uint8_t)(element_size >> 16);
		scratchpad[i++] = (uint8_t)(element_size >> 24);

		crc32 = crc32_calc(crc32, scratchpad, i);
		fwrite(scratchpad, i, 1, dfufp);

		while (element_size)
		{
			crc32 = crc32_calc(crc32, blob->data, blob->count);
			fwrite(blob->data, blob->count, 1, dfufp);
			element_size -= blob->count;
			blob = blob->next;
		}
	}

	/*
	this suffix is the only loosely DFU standards-compliant portion, but ST made it unique to them by changing the bcdDFU value
	*/

	i = 0;
	scratchpad[i++] = 0xFF; // bcdDevice
	scratchpad[i++] = 0xFF;
	scratchpad[i++] = (uint8_t)(0); // idProduct
	scratchpad[i++] = (uint8_t)(0);
	scratchpad[i++] = (uint8_t)(0); // idVendor
	scratchpad[i++] = (uint8_t)(0);
	scratchpad[i++] = 0x1A; // bcdDFU
	scratchpad[i++] = 0x01;
	scratchpad[i++] = 'U'; // ucDfuSignature
	scratchpad[i++] = 'F';
	scratchpad[i++] = 'D';
	scratchpad[i++] = 16; // bLength

	/* the CRC-32 has now been calculated over the entire file, save for the CRC field itself */
	crc32 = crc32_calc(crc32, scratchpad, i);

	scratchpad[i++] = (uint8_t)(crc32 >> 0);
	scratchpad[i++] = (uint8_t)(crc32 >> 8);
	scratchpad[i++] = (uint8_t)(crc32 >> 16);
	scratchpad[i++] = (uint8_t)(crc32 >> 24);

	fwrite(scratchpad, i, 1, dfufp);

	fclose(dfufp);
    dfufp = (FILE *)NULL;

    filesize = fsize(argv[2]); 
    if (filesize <= 0)
    {
		printf("ERROR: file <%s> is errored or has size of zero, can't continue\n", argv[2]);
		return -1;
	}
	dfufp = fopen(argv[2], "rb");
	if (!dfufp)
	{
		printf("ERROR: unable to open file <%s> for reading\n", argv[2]);
		return -1;
	}

    filebuf = (uint8_t *)NULL;
    filebuf = (uint8_t *)malloc(filesize);
    if ((i = fread(filebuf, 1, filesize, dfufp)) != filesize)
    {
		printf("ERROR: read only %d, unable to read full file <%s>\n", i, argv[2]);
		return -1;
	}
    fclose(dfufp);
    dfufp = (FILE *)NULL;

    crc32_clean = crc32_calc(0xFFFFFFFF, filebuf, filesize-16);
   
    cp = strrchr(argv[2],'.');
    if (cp == (char *)NULL)
    {
        fname = (uint8_t *)malloc(strlen(argv[2])+1);
        if (fname == (uint8_t *)NULL)
        {
    		printf("ERROR: unable to allocate memory #1 fname\n");
            return -1;
        }
        fext = (uint8_t *)malloc(3);
        if (fext == (uint8_t *)NULL)
        {
    		printf("ERROR: unable to allocate memory #2 fext\n");
            return -1;
        }
        strcpy((char *)fname, argv[2]);
        strcpy((char *)fext, "dfu");
    }
    else
    {
        lenlen = cp - argv[2];
        if (lenlen < 0) {
            printf("ERROR: cp is before argv[2], pointer error\n");
            return -1;
        }
        fname = (uint8_t *)malloc(lenlen+1);
        if (fname == (uint8_t *)NULL)
        {
    		printf("ERROR: unable to allocate memory #3 fname %ld\n", cp-argv[2]);
            return -1;
        }
        strncpy((char *)fname, argv[2], cp - argv[2]);
        fname[cp-argv[2]] = '\0';
        fext = (uint8_t *)malloc(strlen(cp+1));
        if (fext == (uint8_t *)NULL)
        {
    		printf("ERROR: unable to allocate memory #4 fext\n");
            return -1;
        }
        strcpy((char *)fext, cp+1);
    }
    fullname = (uint8_t *)malloc(strlen((char *)fname)+10+1+strlen((char *)fext));
    if (fullname == (uint8_t *)NULL)
    {
		printf("ERROR: unable to allocate memory #5 fullname\n");
        return -1;
    }

    while ((current_vidpid=removeFromListHead(&head)) != (vidpid_t *)NULL)
    {
        crc32 = crc32_clean;
        i = filesize - 16;

	    filebuf[i++] = 0xFF; // bcdDevice
	    filebuf[i++] = 0xFF;
	    filebuf[i++] = (uint8_t)(current_vidpid->pid>>0); // idProduct
	    filebuf[i++] = (uint8_t)(current_vidpid->pid>>8);
	    filebuf[i++] = (uint8_t)(current_vidpid->vid>>0); // idVendor
	    filebuf[i++] = (uint8_t)(current_vidpid->vid>>8);
    	filebuf[i++] = 0x1A; // bcdDFU
	    filebuf[i++] = 0x01;
    	filebuf[i++] = 'U'; // ucDfuSignature
	    filebuf[i++] = 'F';
	    filebuf[i++] = 'D';
	    filebuf[i++] = 16; // bLength

	/* the CRC-32 has now been calculated over the entire file, save for the CRC field itself */
    	crc32 = crc32_calc(crc32, &filebuf[filesize-16], 12);

	    filebuf[i++] = (uint8_t)(crc32 >> 0);
	    filebuf[i++] = (uint8_t)(crc32 >> 8);
	    filebuf[i++] = (uint8_t)(crc32 >> 16);
	    filebuf[i++] = (uint8_t)(crc32 >> 24);
        sprintf((char *)fullname, "%s_%04x_%04x.%s", (char *)fname, 
                (unsigned int)current_vidpid->vid, 
                (unsigned int)current_vidpid->pid, 
                (char *)fext);
        free(current_vidpid);
        current_vidpid = (vidpid_t *)NULL;
	    dfufp = fopen((char *)fullname, "wb");
     	if (!dfufp)
	    {
		    printf("ERROR: unable to open file <%s> for writing\n", fullname);
		    return -1;
	    }
        bwritten = fwrite(filebuf, 1, filesize, dfufp);
        if (bwritten != filesize)
        {
		    printf("ERROR: unable to write all data to file <%s>\n", fullname);
		    return -1;
        }
        fclose(dfufp);
        dfufp = (FILE *)NULL;
    }
    remove(argv[2]);

    return 0;
}

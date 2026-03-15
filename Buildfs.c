#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

#define MAX_FILES 128
#define FS_NAME_MAX  32
#define FILE_MAX  4096

typedef struct
{
        uint8_t used;
        uint8_t is_dir;
        int parent;
        char name[NAME_MAX];
        char data[FILE_MAX];
        uint32_t size;

} FS;

FS fs[MAX_FILES];
int fs_count = 1;

void add_file(const char *path, int parent)
{
        struct stat st;
        stat(path,&st);

        char *name = strrchr(path,'/');
        if(name) name++;
        else name = (char*)path;

        int id = fs_count++;

        fs[id].used = 1;
        fs[id].parent = parent;
        strcpy(fs[id].name,name);

        if(S_ISDIR(st.st_mode))
        {
                fs[id].is_dir = 1;

                DIR *d = opendir(path);
                struct dirent *e;

                while((e = readdir(d)))
                {
                        if(!strcmp(e->d_name,".") || !strcmp(e->d_name,".."))
                                continue;

                        char p[512];
                        sprintf(p,"%s/%s",path,e->d_name);

                        add_file(p,id);
                }

                closedir(d);
        }
        else
        {
                fs[id].is_dir = 0;

                FILE *f = fopen(path,"rb");
                fs[id].size = fread(fs[id].data,1,FILE_MAX,f);
                fclose(f);
        }
}

int main()
{
        fs[0].used = 1;
        fs[0].is_dir = 1;
        fs[0].parent = 0;
        strcpy(fs[0].name,"/");

        add_file("Root",0);

        FILE *f = fopen("fs.img","wb");
        fwrite(fs,sizeof(fs),1,f);
        fclose(f);

        printf("Filesystem built\n");
}

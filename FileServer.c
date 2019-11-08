#include <stdio.h>
#include <dirent.h>

void PrintFilesInDirectory(char* dir)
{
	struct dirent *de;  // Pointer for directory entry

    	DIR *dr = opendir(dir);

    	if (dr == NULL)  // opendir returns NULL if couldn't open directory
    	{
        	printf("Could not open current directory" );        
    	}

    	while ((de = readdir(dr)) != NULL)
            	printf("%s\n", de->d_name);

    	closedir(dr);
}

int main(void)
{
    PrintFilesInDirectory("./test");
    return 0;
}


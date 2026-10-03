#include <openssl/evp.h>
#include <stdio.h>
#include <string.h>

int main(int argc,char **argv) {
    if(argc!=3 || (strcmp(argv[1],"hash") && strcmp(argv[1],"verify"))) {
        fprintf(stderr,"usage: audit hash|verify <file>\n"); return 2;
    }
    FILE *f=fopen(argv[2],"rb"); if(!f){perror(argv[2]);return 1;}
    EVP_MD_CTX *ctx=EVP_MD_CTX_new(); unsigned char digest[EVP_MAX_MD_SIZE]; unsigned int len;
    EVP_DigestInit_ex(ctx,EVP_sha256(),NULL);
    unsigned char buf[8192]; size_t n;
    while((n=fread(buf,1,sizeof buf,f))>0) EVP_DigestUpdate(ctx,buf,n);
    if(ferror(f)){perror("read");return 1;}
    EVP_DigestFinal_ex(ctx,digest,&len); EVP_MD_CTX_free(ctx); fclose(f);
    char hex[EVP_MAX_MD_SIZE*2+1]; for(unsigned i=0;i<len;i++)sprintf(hex+i*2,"%02x",digest[i]);
    if(!strcmp(argv[1],"hash")) { printf("%s  %s\n",hex,argv[2]); return 0; }
    char expected[EVP_MAX_MD_SIZE*2+1];
    if(scanf("%64s",expected)!=1){fprintf(stderr,"provide expected SHA-256 on stdin\n");return 2;}
    if(strcmp(hex,expected)){fprintf(stderr,"MISMATCH %s\n",hex);return 1;}
    puts("OK"); return 0;
}
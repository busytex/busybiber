#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "perlpack.h"

enum {
    packfs_filefd_min = 1000000000, 
    packfs_filefd_max = 1000001000, 
    packfs_filepath_max_len = 128, 
};
struct packfs_context
{
    int initialized, disabled;
    
    int (*orig_open)(const char *path, int flags);
    int (*orig_close)(int fd);
    ssize_t (*orig_read)(int fd, void* buf, size_t count);
    int (*orig_access)(const char *path, int flags);
    off_t (*orig_lseek)(int fd, off_t offset, int whence);
    int (*orig_stat)(const char *restrict path, struct stat *restrict statbuf);
    int (*orig_fstat)(int fd, struct stat * statbuf);
    FILE* (*orig_fopen)(const char *path, const char *mode);
    int (*orig_fileno)(FILE* stream);
    
    int packfs_filefd[packfs_filefd_max - packfs_filefd_min];
    FILE* packfs_fileptr[packfs_filefd_max - packfs_filefd_min];
    size_t packfs_filesize[packfs_filefd_max - packfs_filefd_min];
    
    size_t packfs_builtin_files_num;
    char packfs_builtin_prefix[packfs_filepath_max_len];
    const char** packfs_builtin_starts;
    const char** packfs_builtin_ends;
    const char** packfs_builtin_safepaths;
    const char** packfs_builtin_abspaths;
};

struct packfs_context* packfs_ensure_context()
{
    static struct packfs_context packfs_ctx = {0};

    if(packfs_ctx.initialized != 1)
    {
        extern int orig_open(const char *path, int flags); packfs_ctx.orig_open = orig_open;
        extern int orig_close(int fd); packfs_ctx.orig_close = orig_close;
        extern ssize_t orig_read(int fd, void* buf, size_t count); packfs_ctx.orig_read = orig_read;
        extern int orig_access(const char *path, int flags); packfs_ctx.orig_access = orig_access;
        extern off_t orig_lseek(int fd, off_t offset, int whence); packfs_ctx.orig_lseek = orig_lseek;
        extern int orig_stat(const char *restrict path, struct stat *restrict statbuf); packfs_ctx.orig_stat = orig_stat;
        extern int orig_fstat(int fd, struct stat * statbuf); packfs_ctx.orig_fstat = orig_fstat;
        extern FILE* orig_fopen(const char *path, const char *mode); packfs_ctx.orig_fopen = orig_fopen;
        extern int orig_fileno(FILE* stream); packfs_ctx.orig_fileno = orig_fileno;
        // TODO: append / if missing
#define PACKFS_STRING_VALUE_(x) #x
#define PACKFS_STRING_VALUE(x) PACKFS_STRING_VALUE_(x)
        strcpy(packfs_ctx.packfs_builtin_prefix,
#ifdef PACKFS_BUILTIN_PREFIX
            PACKFS_STRING_VALUE(PACKFS_BUILTIN_PREFIX)
#else
        ""
#endif
        );
#undef PACKFS_STRING_VALUE_
#undef PACKFS_STRING_VALUE

        packfs_ctx.packfs_builtin_files_num = 0;
        packfs_ctx.packfs_builtin_starts = NULL;
        packfs_ctx.packfs_builtin_ends = NULL;
        packfs_ctx.packfs_builtin_safepaths = NULL;
        packfs_ctx.packfs_builtin_abspaths = NULL;
        
        packfs_ctx.initialized = 1;
        packfs_ctx.disabled = 1;

#ifdef PACKFS_BUILTIN_PREFIX
        packfs_ctx.disabled = 0;
        packfs_ctx.packfs_builtin_files_num = packfs_builtin_files_num;
        packfs_ctx.packfs_builtin_starts = packfs_builtin_starts;
        packfs_ctx.packfs_builtin_ends = packfs_builtin_ends;
        packfs_ctx.packfs_builtin_safepaths = packfs_builtin_safepaths;
        packfs_ctx.packfs_builtin_abspaths = packfs_builtin_abspaths;
#endif
    }
    
    return &packfs_ctx;
}

void packfs_sanitize_path(char* path_sanitized, const char* path)
{
    size_t len = path != NULL ? strlen(path) : 0;
    if(len == 0)
        path_sanitized[0] = '\0';

    for(int i = (path != NULL && len > 2 && path[0] == '.' && path[1] == '/') ? 2 : 0, k = 0; len > 0 && i < len; i++)
    {
        if(!(i > 1 && path[i] == '/' && path[i - 1] == '/'))
        {
            path_sanitized[k++] = path[i];
            path_sanitized[k] = '\0';
        }
    }
}

int packfs_strncmp(const char* prefix, const char* path, size_t count)
{
    return (prefix != NULL && prefix[0] != '\0' && path != NULL && path[0] != '\0') ? strncmp(prefix, path, count) : 1;
}

int packfs_open(struct packfs_context* packfs_ctx, const char* path, FILE** out)
{
    char path_sanitized[packfs_filepath_max_len]; packfs_sanitize_path(path_sanitized, path);

    FILE* fileptr = NULL;
    size_t filesize = 0;
    
    if(packfs_ctx->packfs_builtin_files_num > 0 && 0 == packfs_strncmp(packfs_ctx->packfs_builtin_prefix, path_sanitized, strlen(packfs_ctx->packfs_builtin_prefix)))
    {
        for(size_t i = 0; i < packfs_ctx->packfs_builtin_files_num; i++)
        {
            if(0 == strcmp(path_sanitized, packfs_ctx->packfs_builtin_abspaths[i]))
            {
                filesize = (size_t)(packfs_ctx->packfs_builtin_ends[i] - packfs_ctx->packfs_builtin_starts[i]);
                fileptr = fmemopen((void*)packfs_ctx->packfs_builtin_starts[i], filesize, "r");
                break;
            }
        }
    }

    if(out != NULL)
        *out = fileptr;

    for(size_t k = 0; fileptr != NULL && k < packfs_filefd_max - packfs_filefd_min; k++)
    {
        if(packfs_ctx->packfs_filefd[k] == 0)
        {
            packfs_ctx->packfs_filefd[k] = packfs_filefd_min + k;
            packfs_ctx->packfs_fileptr[k] = fileptr;
            packfs_ctx->packfs_filesize[k] = filesize;
            return packfs_ctx->packfs_filefd[k];
        }
    }

    return -1;
}

int packfs_close(struct packfs_context* packfs_ctx, int fd)
{
    if(fd < packfs_filefd_min || fd >= packfs_filefd_max)
        return -2;

    for(size_t k = 0; k < packfs_filefd_max - packfs_filefd_min; k++)
    {
        if(packfs_ctx->packfs_filefd[k] == fd)
        {
            packfs_ctx->packfs_filefd[k] = 0;
            packfs_ctx->packfs_filesize[k] = 0;
            int res = fclose(packfs_ctx->packfs_fileptr[k]);
            packfs_ctx->packfs_fileptr[k] = NULL;
            return res;
        }
    }
    return -2;
}

void* packfs_find(struct packfs_context* packfs_ctx, int fd, FILE* ptr)
{
    if(ptr != NULL)
    {
        for(size_t k = 0; k < packfs_filefd_max - packfs_filefd_min; k++)
        {
            if(packfs_ctx->packfs_fileptr[k] == ptr)
                return &packfs_ctx->packfs_filefd[k];
        }
        return NULL;
    }
    else
    {
        if(fd < packfs_filefd_min || fd >= packfs_filefd_max)
            return NULL;
        
        for(size_t k = 0; k < packfs_filefd_max - packfs_filefd_min; k++)
        {
            if(packfs_ctx->packfs_filefd[k] == fd)
                return packfs_ctx->packfs_fileptr[k];
        }
    }
    return NULL;
}

ssize_t packfs_read(struct packfs_context* packfs_ctx, int fd, void* buf, size_t count)
{
    FILE* ptr = packfs_find(packfs_ctx, fd, NULL);
    if(!ptr)
        return -1;
    return (ssize_t)fread(buf, 1, count, ptr);
}

int packfs_seek(struct packfs_context* packfs_ctx, int fd, long offset, int whence)
{
    FILE* ptr = packfs_find(packfs_ctx, fd, NULL);
    if(!ptr)
        return -1;
    return fseek(ptr, offset, whence);
}

int packfs_access(struct packfs_context* packfs_ctx, const char* path)
{
    char path_sanitized[packfs_filepath_max_len]; packfs_sanitize_path(path_sanitized, path);

    if(0 == packfs_strncmp(packfs_ctx->packfs_builtin_prefix, path_sanitized, strlen(packfs_ctx->packfs_builtin_prefix)))
    {
        for(size_t i = 0; i < packfs_ctx->packfs_builtin_files_num; i++)
        {
            if(0 == strcmp(path_sanitized, packfs_ctx->packfs_builtin_abspaths[i]))
                return 0;
        }
        return -1;
    }
    
    return -2;
}

int packfs_stat(struct packfs_context* packfs_ctx, const char* path, int fd, struct stat *restrict statbuf)
{
    char path_sanitized[packfs_filepath_max_len]; packfs_sanitize_path(path_sanitized, path);
    
    if(0 == packfs_strncmp(packfs_ctx->packfs_builtin_prefix, path_sanitized, strlen(packfs_ctx->packfs_builtin_prefix)))
    {
        for(size_t i = 0; i < packfs_ctx->packfs_builtin_files_num; i++)
        {
            if(0 == strcmp(path_sanitized, packfs_ctx->packfs_builtin_abspaths[i]))
            {
                *statbuf = (struct stat){0};
                //if(packfs_builtin[i].isdir)
                //{
                //    statbuf->st_size = 0;
                //    statbuf->st_mode = S_IFDIR;
                //}
                //else
                {
                    statbuf->st_size = (off_t)(packfs_ctx->packfs_builtin_ends[i] - packfs_ctx->packfs_builtin_starts[i]);
                    statbuf->st_mode = S_IFREG;
                }
                return 0;
            }
        }
        return -1;
    }
    
    if(fd >= 0 && packfs_filefd_min <= fd && fd < packfs_filefd_max)
    {
        for(size_t k = 0; k < packfs_filefd_max - packfs_filefd_min; k++)
        {
            if(packfs_ctx->packfs_filefd[k] == fd)
            {
                *statbuf = (struct stat){0};
                statbuf->st_size = packfs_ctx->packfs_filesize[k];
                statbuf->st_mode = S_IFREG;
                return 0;
            }
        }
        return -1;
    }

    return -2;
}

///////////

FILE* fopen(const char *path, const char *mode)
{
    struct packfs_context* packfs_ctx = packfs_ensure_context();
    if(!packfs_ctx->disabled)
    {
        FILE* res = NULL;
        if(packfs_open(packfs_ctx, path, &res) >= 0)
        {
            return res;
        }
    }

    FILE* res = packfs_ctx->orig_fopen(path, mode);
    return res;
}

int fileno(FILE *stream)
{
    struct packfs_context* packfs_ctx = packfs_ensure_context();
    
    int res = packfs_ctx->orig_fileno(stream);
    
    if(!packfs_ctx->disabled && res < 0)
    {        
        int* ptr = packfs_find(packfs_ctx, -1, stream);
        res = ptr == NULL ? -1 : (*ptr);
    }
    
    return res;
}

int open(const char *path, int flags, ...)
{
    struct packfs_context* packfs_ctx = packfs_ensure_context();
    if(!packfs_ctx->disabled)
    {
        int res = packfs_open(packfs_ctx, path, NULL);
        if(res >= 0)
        { 
            return res;
        }
    }
    
    int res = packfs_ctx->orig_open(path, flags);
    return res;
}

int close(int fd)
{
    struct packfs_context* packfs_ctx = packfs_ensure_context();
    if(!packfs_ctx->disabled)
    {
        int res = packfs_close(packfs_ctx, fd);
        if(res >= -1)
        {
            return res;
        }
    }
    
    int res = packfs_ctx->orig_close(fd);
    return res;
}


ssize_t read(int fd, void* buf, size_t count)
{
    struct packfs_context* packfs_ctx = packfs_ensure_context();
    if(!packfs_ctx->disabled)
    {
        ssize_t res = packfs_read(packfs_ctx, fd, buf, count);
        if(res >= 0)
        {
            return res;
        }
    }

    ssize_t res = packfs_ctx->orig_read(fd, buf, count);
    return res;
}

off_t lseek(int fd, off_t offset, int whence)
{
    struct packfs_context* packfs_ctx = packfs_ensure_context();
    if(!packfs_ctx->disabled)
    {
        int res = packfs_seek(packfs_ctx, fd, (long)offset, whence);
        if(res >= 0)
        {
            return res;
        }
    }

    off_t res = packfs_ctx->orig_lseek(fd, offset, whence);
    return res;
}


int access(const char *path, int flags) 
{
    struct packfs_context* packfs_ctx = packfs_ensure_context();
    if(!packfs_ctx->disabled)
    {
        int res = packfs_access(packfs_ctx, path);
        if(res >= -1)
        {
            return res;
        }
    }
    
    int res = packfs_ctx->orig_access(path, flags); 
    return res;
}

int stat(const char *restrict path, struct stat *restrict statbuf)
{
    struct packfs_context* packfs_ctx = packfs_ensure_context();
    if(!packfs_ctx->disabled)
    {
        int res = packfs_stat(packfs_ctx, path, -1, statbuf);
        if(res >= -1)
        {
            return res;
        }
    }

    int res = packfs_ctx->orig_stat(path, statbuf);
    return res;
}

int fstat(int fd, struct stat * statbuf)
{
    struct packfs_context* packfs_ctx = packfs_ensure_context();
    if(!packfs_ctx->disabled)
    {
        int res = packfs_stat(packfs_ctx, NULL, fd, statbuf);
        if(res >= -1)
        {
            return res;
        }
    }
    
    int res = packfs_ctx->orig_fstat(fd, statbuf);
    return res;
}

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

// #include <xsinit.c>
void xs_init(pTHX) //EXTERN_C 
{
    static const char file[] = __FILE__;
    dXSUB_SYS;
    PERL_UNUSED_CONTEXT;
    
    extern void boot_DynaLoader(pTHX_ CV* cv); newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
   
    extern void boot_mro(pTHX_ CV* cv); newXS("mro::bootstrap", boot_mro, file);
    extern void boot_Devel__Peek(pTHX_ CV* cv); newXS("Devel::Peek", boot_Devel__Peek, file);
    extern void boot_File__DosGlob(pTHX_ CV* cv); newXS("File::DosGlob::bootstrap", boot_File__DosGlob, file);
    extern void boot_File__Glob(pTHX_ CV* cv); newXS("File::Glob::bootstrap", boot_File__Glob, file);
    extern void boot_Sys__Syslog(pTHX_ CV* cv); newXS("Sys::Syslog::bootstrap", boot_Sys__Syslog, file);
    extern void boot_Sys__Hostname(pTHX_ CV* cv); newXS("Sys::Hostname::bootstrap", boot_Sys__Hostname, file);
    extern void boot_PerlIO__via(pTHX_ CV* cv); newXS("PerlIO::via::bootstrap", boot_PerlIO__via, file);
    extern void boot_PerlIO__mmap(pTHX_ CV* cv); newXS("PerlIO::mmap::bootstrap", boot_PerlIO__mmap, file);
    extern void boot_PerlIO__encoding(pTHX_ CV* cv); newXS("PerlIO::encoding::bootstrap", boot_PerlIO__encoding, file);
    //extern void boot_PerlIO__scalar(pTHX_ CV* cv); newXS("PerlIO::scalar::bootstrap", boot_PerlIO__scalar, file);
    //extern void boot_PerlIO__utf8_strict(pTHX_ CV* cv); newXS("PerlIO::utf8_strict::bootstrap", boot_PerlIO__utf8_strict, file);
    extern void boot_B(pTHX_ CV* cv); newXS("B::bootstrap", boot_B, file);
    extern void boot_attributes(pTHX_ CV* cv); newXS("attributes::bootstrap", boot_attributes, file);
    extern void boot_Unicode__Normalize(pTHX_ CV* cv); newXS("Unicode::Normalize::bootstrap", boot_Unicode__Normalize, file);
    extern void boot_Unicode__Collate(pTHX_ CV* cv); newXS("Unicode::Collate::bootstrap", boot_Unicode__Collate, file);
    extern void boot_Unicode__LineBreak(pTHX_ CV* cv); newXS("Unicode::LineBreak::bootstrap", boot_Unicode__LineBreak, file);
    extern void boot_threads(pTHX_ CV* cv); newXS("threads::bootstrap", boot_threads, file);
    extern void boot_threads__shared(pTHX_ CV* cv); newXS("threads::shared::bootstrap", boot_threads__shared, file);
    extern void boot_IPC__SysV(pTHX_ CV* cv); newXS("IPC::SysV::bootstrap", boot_IPC__SysV, file);
    extern void boot_re(pTHX_ CV* cv); newXS("re::bootstrap", boot_re, file);
    extern void boot_Digest__MD5(pTHX_ CV* cv); newXS("Digest::MD5::bootstrap", boot_Digest__MD5, file);
    extern void boot_Digest__SHA(pTHX_ CV* cv); newXS("Digest::SHA::bootstrap", boot_Digest__SHA, file);
    extern void boot_SDBM_File(pTHX_ CV* cv); newXS("SDBM_File::bootstrap", boot_SDBM_File, file);
    extern void boot_Math__BigInt__FastCalc(pTHX_ CV* cv); newXS("Math::BigInt::FastCalc::bootstrap", boot_Math__BigInt__FastCalc, file);
    extern void boot_Data__Dumper(pTHX_ CV* cv); newXS("Data::Dumper::bootstrap", boot_Data__Dumper, file);
    extern void boot_I18N__Langinfo(pTHX_ CV* cv); newXS("I18N::Langinfo::bootstrap", boot_I18N__Langinfo, file);
    extern void boot_Time__HiRes(pTHX_ CV* cv); newXS("Time::HiRes::bootstrap", boot_Time__HiRes, file);
    extern void boot_Time__Piece(pTHX_ CV* cv); newXS("Time::Piece::bootstrap", boot_Time__Piece, file);
    extern void boot_IO(pTHX_ CV* cv); newXS("IO::bootstrap", boot_IO, file);
    extern void boot_Socket(pTHX_ CV* cv); newXS("Socket::bootstrap", boot_Socket, file);
    extern void boot_Hash__Util__FieldHash(pTHX_ CV* cv); newXS("Hash::Util::FieldHash::bootstrap", boot_Hash__Util__FieldHash, file); 
    extern void boot_Hash__Util(pTHX_ CV* cv); newXS("Hash::Util::bootstrap", boot_Hash__Util, file);
    extern void boot_Filter__Util__Call(pTHX_ CV* cv); newXS("Filter::Util::Call::bootstrap", boot_Filter__Util__Call, file);
    extern void boot_POSIX(pTHX_ CV* cv); newXS("POSIX::bootstrap", boot_POSIX, file);
    extern void boot_Encode__Unicode(pTHX_ CV* cv); newXS("Encode::Unicode::bootstrap", boot_Encode__Unicode, file);
    extern void boot_Encode(pTHX_ CV* cv); newXS("Encode::bootstrap", boot_Encode, file);
    extern void boot_Encode__JP(pTHX_ CV* cv); newXS("Encode::JP::bootstrap", boot_Encode__JP, file);
    extern void boot_Encode__KR(pTHX_ CV* cv); newXS("Encode::KR::bootstrap", boot_Encode__KR, file);
    extern void boot_Encode__EBCDIC(pTHX_ CV* cv); newXS("Encode::EBCDIC::bootstrap", boot_Encode__EBCDIC, file);
    extern void boot_Encode__CN(pTHX_ CV* cv); newXS("Encode::CN::bootstrap", boot_Encode__CN, file);
    extern void boot_Encode__Symbol(pTHX_ CV* cv); newXS("Encode::Symbol::bootstrap", boot_Encode__Symbol, file);
    extern void boot_Encode__Byte(pTHX_ CV* cv); newXS("Encode::Byte::bootstrap", boot_Encode__Byte, file);
    extern void boot_Encode__TW(pTHX_ CV* cv); newXS("Encode::TW::bootstrap", boot_Encode__TW, file);
    extern void boot_Encode__EUCJPASCII(pTHX_ CV* cv); newXS("Encode::EUCJPASCII::bootstrap", boot_Encode__EUCJPASCII, file);
    extern void boot_Encode__JIS2K(pTHX_ CV* cv); newXS("Encode::JIS2K::bootstrap", boot_Encode__JIS2K, file);
    extern void boot_Encode__HanExtra(pTHX_ CV* cv); newXS("Encode::HanExtra::bootstrap", boot_Encode__HanExtra, file);
    extern void boot_Compress__Raw__Zlib(pTHX_ CV* cv); newXS("Compress::Raw::Zlib::bootstrap", boot_Compress__Raw__Zlib, file);
    extern void boot_Compress__Raw__Bzip2(pTHX_ CV* cv); newXS("Compress::Raw::Bzip2::bootstrap", boot_Compress__Raw__Bzip2, file);
    extern void boot_MIME__Base64(pTHX_ CV* cv); newXS("MIME::Base64::bootstrap", boot_MIME__Base64, file);
    extern void boot_Cwd(pTHX_ CV* cv); newXS("Cwd::bootstrap", boot_Cwd, file);
    extern void boot_Storable(pTHX_ CV* cv); newXS("Storable::bootstrap", boot_Storable, file);
    extern void boot_List__Util(pTHX_ CV* cv); newXS("List::Util::bootstrap", boot_List__Util, file);
    //extern void boot_List__SomeUtils(pTHX_ CV* cv); newXS("List::SomeUtils::bootstrap", boot_List__SomeUtils, file);
    //extern void boot_List__MoreUtils(pTHX_ CV* cv); newXS("List::MoreUtils::bootstrap", boot_List__MoreUtils, file);
    extern void boot_Fcntl(pTHX_ CV* cv); newXS("Fcntl::bootstrap", boot_Fcntl, file);
    extern void boot_Opcode(pTHX_ CV* cv); newXS("Opcode::bootstrap", boot_Opcode, file);
    
    extern void boot_DateTime(pTHX_ CV* cv); newXS("DateTime::bootstrap", boot_DateTime, file);
    extern void boot_Clone(pTHX_ CV* cv); newXS("Clone::bootstrap", boot_Clone, file);
    extern void boot_autovivification(pTHX_ CV* cv); newXS("autovivification::bootstrap", boot_autovivification, file);
    extern void boot_PadWalker(pTHX_ CV* cv); newXS("PadWalker::bootstrap", boot_PadWalker, file);

    extern void boot_Devel__Caller(pTHX_ CV* cv); newXS("Devel::Caller::bootstrap", boot_Devel__Caller, file);
    extern void boot_Devel__LexAlias(pTHX_ CV* cv); newXS("Devel::LexAlias::bootstrap", boot_Devel__LexAlias, file);
    extern void boot_Params__Util(pTHX_ CV* cv); newXS("Params::Util::bootstrap", boot_Params__Util, file);
    extern void boot_HTML__Parser(pTHX_ CV* cv); newXS("HTML::Parser::bootstrap", boot_HTML__Parser, file);
    extern void boot_Class__XSAccessor(pTHX_ CV* cv); newXS("Class::XSAccessor::bootstrap", boot_Class__XSAccessor, file);
    extern void boot_Sort__Key(pTHX_ CV* cv); newXS("Sort::Key::bootstrap", boot_Sort__Key, file);
    extern void boot_Variable__Magic(pTHX_ CV* cv); newXS("Variable::Magic::bootstrap", boot_Variable__Magic, file);

    extern void boot_XML__LibXML(pTHX_ CV* cv); newXS("XML::LibXML::bootstrap", boot_XML__LibXML, file);
    extern void boot_XML__LibXSLT(pTHX_ CV* cv); newXS("XML::LibXSLT::bootstrap", boot_XML__LibXSLT, file);
    extern void boot_XML__Parser__Expat(pTHX_ CV* cv); newXS("XML::Parser::Expat::bootstrap", boot_XML__Parser__Expat, file); 

    extern void boot_Text__BibTeX(pTHX_ CV* cv); newXS("Text::BibTeX::bootstrap", boot_Text__BibTeX, file);
    // extern void boot_Text__CSV_XS(pTHX_ CV* cv); newXS("Text::CSV_XS::bootstrap", boot_Text__CSV_XS, file);
    // extern void boot_DBI(pTHX_ CV* cv); newXS("DBI::bootstrap", boot_DBI, file);
    // extern void boot_DBD__SQLite(pTHX_ CV* cv); newXS("DBD::SQLite::bootstrap", boot_DBD__SQLite, file);
    // extern void boot_Net__SSLeay(pTHX_ CV* cv); newXS("Net::SSLeay::bootstrap", boot_Net__SSLeay, file);
    // extern void boot_Package__Stash__XS(pTHX_ CV* cv); newXS("Package::Stash::XS::bootstrap", boot_Package__Stash__XS, file); 
    // extern void boot_Params__Validate__XS(pTHX_ CV* cv); newXS("Params::Validate::XS::bootstrap", boot_Params__Validate__XS, file); 
    // extern void boot_Filter__Util__Call(pTHX_ CV* cv); newXS("Filter::Util::Call::bootstrap", boot_Filter__Util__Call, file); 
    // extern void boot_IO__Compress__Brotli(pTHX_ CV* cv); newXS("IO::Compress::Brotli::bootstrap", boot_IO__Compress__Brotli, file); 
}

int main(int argc, char *argv[], char* envp[])
{
    if(argc < 1)
        return -1;
    
    struct packfs_context* packfs_ctx = packfs_ensure_context();
    
    static char bin_biber[packfs_filepath_max_len];
    static char* myperl_argv[128];
    strcpy(bin_biber, packfs_ctx->packfs_builtin_prefix);
    strcat(bin_biber, "/bin/biber");
    myperl_argv[0] = argv[0];
    myperl_argv[1] = bin_biber;
    for(int i = 1; i < argc; i++) myperl_argv[1 + i] = argv[i];
    int myperl_argc = argc + 1;
    
    PERL_SYS_INIT3(&argc, &argv, &envp);
    PerlInterpreter* myperl = perl_alloc();
    if(myperl == NULL)
        return -1;
    perl_construct(myperl);
    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;

    int res = perl_parse(myperl, xs_init, myperl_argc, myperl_argv, envp);
    if(res == 0)
        res = perl_run(myperl); // error if res != 0 (or stricter in case exit(0) was called from perl code): (res & 0xFF) != 0: 

    PL_perl_destruct_level = 0;
    res = perl_destruct(myperl);
    perl_free(myperl);
    PERL_SYS_TERM();

    return res;
}

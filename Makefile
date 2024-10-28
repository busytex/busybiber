OBJCOPY = objcopy
STATICLDFLAGS = --static -static -static-libstdc++ -static-libgcc 

URLPERL = https://www.cpan.org/src/5.0/perl-5.35.4.tar.gz

MODULES_def = -DPERLPACK_mro -DPERLPACK_Devel__Peek -DPERLPACK_File__DosGlob -DPERLPACK_File__Glob -DPERLPACK_Sys__Syslog -DPERLPACK_Sys__Hostname -DPERLPACK_PerlIO__via -DPERLPACK_PerlIO__mmap -DPERLPACK_PerlIO__encoding -DPERLPACK_PerlIO__scalar -DPERLPACK_B -DPERLPACK_attributes -DPERLPACK_Unicode__Normalize -DPERLPACK_Unicode__Collate -DPERLPACK_threads -DPERLPACK_threads__shared -DPERLPACK_IPC__SysV -DPERLPACK_re -DPERLPACK_Digest__MD5 -DPERLPACK_Digest__SHA -DPERLPACK_SDBM_File -DPERLPACK_Math__BigInt__FastCalc -DPERLPACK_Data__Dumper -DPERLPACK_I18N__Langinfo -DPERLPACK_Time__HiRes -DPERLPACK_Time__Piece -DPERLPACK_IO -DPERLPACK_Socket -DPERLPACK_Hash__Util__FieldHash -DPERLPACK_Hash__Util -DPERLPACK_Filter__Util__Call -DPERLPACK_POSIX -DPERLPACK_Encode__Unicode -DPERLPACK_Encode -DPERLPACK_Encode__JP -DPERLPACK_Encode__KR -DPERLPACK_Encode__CN -DPERLPACK_Encode__Symbol -DPERLPACK_Encode__Byte -DPERLPACK_Encode__TW -DPERLPACK_Compress__Raw__Zlib -DPERLPACK_Compress__Raw__Bzip2 -DPERLPACK_MIME__Base64 -DPERLPACK_Cwd -DPERLPACK_Storable -DPERLPACK_List__Util -DPERLPACK_Fcntl -DPERLPACK_Opcode
MODULES_a = mro/mro.a Devel/Peek/Peek.a File/DosGlob/DosGlob.a File/Glob/Glob.a Sys/Syslog/Syslog.a Sys/Hostname/Hostname.a PerlIO/via/via.a PerlIO/mmap/mmap.a PerlIO/encoding/encoding.a PerlIO/scalar/scalar.a B/B.a attributes/attributes.a Unicode/Normalize/Normalize.a Unicode/Collate/Collate.a threads/threads.a threads/shared/shared.a IPC/SysV/SysV.a re/re.a Digest/MD5/MD5.a Digest/SHA/SHA.a SDBM_File/SDBM_File.a Math/BigInt/FastCalc/FastCalc.a Data/Dumper/Dumper.a I18N/Langinfo/Langinfo.a Time/HiRes/HiRes.a Time/Piece/Piece.a IO/IO.a Socket/Socket.a Hash/Util/FieldHash/FieldHash.a Hash/Util/Util.a Filter/Util/Call/Call.a POSIX/POSIX.a Encode/Unicode/Unicode.a Encode/Encode.a Encode/JP/JP.a Encode/KR/KR.a Encode/EBCDIC/EBCDIC.a Encode/CN/CN.a Encode/Symbol/Symbol.a Encode/Byte/Byte.a Encode/TW/TW.a Compress/Raw/Zlib/Zlib.a Compress/Raw/Bzip2/Bzip2.a MIME/Base64/Base64.a Cwd/Cwd.a Storable/Storable.a List/Util/Util.a Fcntl/Fcntl.a Opcode/Opcode.a
MODULES_ext = mro Devel/Peek File/DosGlob File/Glob Sys/Syslog Sys/Hostname PerlIO/via PerlIO/mmap PerlIO/encoding PerlIO/scalar B attributes Unicode/Normalize Unicode/Collate threads threads/shared IPC/SysV re Digest/MD5 Digest/SHA SDBM_File Math/BigInt/FastCalc Data/Dumper I18N/Langinfo Time/HiRes Time/Piece IO Socket Hash/Util/FieldHash Hash/Util Filter/Util/Call POSIX Encode/Unicode Encode Encode/JP Encode/KR Encode/EBCDIC Encode/CN Encode/Symbol Encode/Byte Encode/TW Compress/Raw/Zlib Compress/Raw/Bzip2 MIME/Base64 Cwd Storable List/Util Fcntl Opcode   

build/libperl.a:
	mkdir -p build
	curl -L $(URLPERL) | tar -xzf - --strip-components=1 --directory=build
	cd build && sh ./Configure -sde -Dman1dir=none -Dman3dir=none -Dprefix=/mnt/perlpack -Dinstallprefix=../packfs -Aldflags=-lm -Accflags=-lm -Dusedevel -Dlibs="-lpthread -ldl -lm -lutil -lc" -Dstatic_ext="$(MODULES_ext)" && cd ..
	$(MAKE) -C build
	$(MAKE) -C build install
	#$(MAKE) -C build miniperl generate_uudmap
	#$(MAKE) -C build perl
	#$(MAKE) -C build install

libc_perlpack.a:
	cp $(shell $(CC) -print-file-name=libc.a) $@
	$(AR) x $@  open.lo close.lo read.lo stat.lo fstat.lo lseek.lo access.lo fopen.lo fileno.lo
	$(OBJCOPY) --redefine-sym open=orig_open     open.lo
	$(OBJCOPY) --redefine-sym close=orig_close   close.lo
	$(OBJCOPY) --redefine-sym read=orig_read     read.lo
	$(OBJCOPY) --redefine-sym stat=orig_stat     stat.lo
	$(OBJCOPY) --redefine-sym fstat=orig_fstat   fstat.lo
	$(OBJCOPY) --redefine-sym lseek=orig_lseek   lseek.lo
	$(OBJCOPY) --redefine-sym access=orig_access access.lo
	$(OBJCOPY) --redefine-sym fopen=orig_fopen   fopen.lo
	$(OBJCOPY) --redefine-sym fileno=orig_fileno fileno.lo
	$(AR) rs $@ open.lo close.lo read.lo stat.lo fstat.lo lseek.lo access.lo fopen.lo fileno.lo

perlpackstatic:
	-rm -rf packfs/man packfs/lib/*/pod/
	-find packfs -name '*.pod' -o -name '*.ld' -o -name '*.a' -o -name '*.h' -delete
	perl perlpack.pl -i packfs -o perlpack.h --prefix=/mnt/perlpack/ --ld="$(LD)"
	cp perlpack.pl myscript.pl && $(LD) -r -b binary -o myscript.o myscript.pl
	$(CC) -o $@ perlpack.c myscript.o -DPACKFS_STATIC -DPACKFS_BUILTIN_PREFIX=/mnt/perlpack/  -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -I$(PWD)/build -I/usr/local/include   -Wl,-E -fstack-protector-strong -fwrapv -fno-strict-aliasing -L/usr/local/lib build/libperl.a libc_perlpack.a -lpthread -ldl -lm -lutil $(STATICLDFLAGS)  $(MODULES_def) $(shell printf "build/lib/auto/%s " $(MODULES_a)) @perlpack.h.txt

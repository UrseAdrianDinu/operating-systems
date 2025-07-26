CFLAGS = /W4 /EHsc /Za /nologo /MD 

build: so-cpp.obj queue.obj hashmap.obj
	cl $(CFLAGS) so-cpp.obj queue.obj hashmap.obj /Feso-cpp.exe

so-cpp.obj: so-cpp.c
	cl $(CFLAGS) /Foso-cpp.obj /c so-cpp.c

queue.obj: queue.c
	cl $(CFLAFS) /Foqueue.obj /c queue.c

hashmap.obj: hashmap.c
	cl $(CFLAFS) /Fohashmap.obj /c hashmap.c

clean:
	del *.obj so-cpp.exe

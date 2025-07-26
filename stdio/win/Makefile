
build: so_stdio.obj
	link /nologo /dll /out:so_stdio.dll /implib:so_stdio.lib so_stdio.obj

so_stdio.obj: so_stdio.c
	cl /c so_stdio.c

clean:
	del *.obj so_stdio.dll so_stdio.lib
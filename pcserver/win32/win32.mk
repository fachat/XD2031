DISTDIR=xd2031-win32

# Create a zipfile for binary distribution
dist:	$(TARGET)
	make -C ../imgtool WIN=y clean
	make -C ../imgtool WIN=y
	mkdir -p $(DISTDIR)
	cp -r ../sample $(DISTDIR)
	cp -r ../tools $(DISTDIR)
	cp $(TARGET) $(DISTDIR)
	cp COPYING $(DISTDIR)/COPYING.txt
	cp StartServer.bat $(DISTDIR)
	cp ../imgtool/imgtool.exe $(DISTDIR)
	find $(DISTDIR) \( -iname *.txt -o -iname *.bat \) -exec unix2dos {} \;
	zip -r $(DISTDIR)-$$(date +%Y-%m-%d).zip $(DISTDIR)

distclean:
	rm -rf $(DISTDIR)

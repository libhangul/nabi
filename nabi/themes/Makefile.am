THEMES = \
	SimplyRed \
	MSWindows \
	Mac \
	keyboard \
	Onion \
	KingSejong

THEMES_DIR = @NABI_THEMES_DIR@

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(THEMES_DIR); \
	for dir in $(THEMES) ; do \
		$(mkinstalldirs) $(DESTDIR)$(THEMES_DIR)/$$dir; \
		$(INSTALL_DATA) $(srcdir)/$$dir/none.png $(DESTDIR)$(THEMES_DIR)/$$dir; \
		$(INSTALL_DATA) $(srcdir)/$$dir/hangul.png $(DESTDIR)$(THEMES_DIR)/$$dir; \
		$(INSTALL_DATA) $(srcdir)/$$dir/english.png $(DESTDIR)$(THEMES_DIR)/$$dir; \
		test -f $(srcdir)/$$dir/README && \
			$(INSTALL_DATA) $(srcdir)/$$dir/README $(DESTDIR)$(THEMES_DIR)/$$dir; \
	done

EXTRA_DIST = \
	MSWindows/english.png		\
	MSWindows/hangul.png		\
	MSWindows/none.png		\
	Mac/english.png			\
	Mac/hangul.png			\
	Mac/none.png			\
	SimplyRed/english.png		\
	SimplyRed/hangul.png		\
	SimplyRed/none.png		\
	Onion/english.png		\
	Onion/hangul.png		\
	Onion/none.png			\
	Onion/README			\
	keyboard/english.png		\
	keyboard/hangul.png		\
	keyboard/none.png		\
	keyboard/README			\
	KingSejong/english.png		\
	KingSejong/hangul.png		\
	KingSejong/none.png		\
	KingSejong/README

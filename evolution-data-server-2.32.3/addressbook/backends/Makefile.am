if ENABLE_LDAP
LDAP_SUBDIR = ldap
else
LDAP_SUBDIR =
endif

SUBDIRS = file vcf $(LDAP_SUBDIR) groupwise webdav

-include $(top_srcdir)/git.mk

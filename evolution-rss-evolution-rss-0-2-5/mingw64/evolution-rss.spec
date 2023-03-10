Summary: Evolution RSS Reader
Name: evolution-rss
Version: 0.2.6
Release: 1%{?dist}
Group: Applications/Internet
License: GPLv2 and GPLv2+
Source: http://gnome.eu.org/%{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
URL: http://gnome.eu.org/evo/index.php/Evolution_RSS_Reader_Plugin
Requires: evolution

Requires(pre): GConf2
Requires(post): GConf2
Requires(preun): GConf2

BuildRequires: gettext
BuildRequires: evolution-devel
BuildRequires: evolution-data-server-devel 
BuildRequires: dbus-glib-devel
#BuildRequires: gecko-devel
BuildRequires: perl(XML::Parser)
BuildRequires: libtool

%description
This is an evolution plugin which enables evolution to read rss feeds.

%prep
%setup -q -n evolution-rss-%{version}

%build
%configure
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
export GCONF_DISABLE_MAKEFILE_SCHEMA_INSTALL=1
make install DESTDIR="%{buildroot}" INSTALL="install -p"
find %{buildroot} -name \*\.la -print | xargs rm -f
%find_lang %{name}

%clean
rm -rf %{buildroot}

%pre
if [ "$1" -gt 1 ]; then
	export GCONF_CONFIG_SOURCE=`gconftool-2 --get-default-source`
	gconftool-2 --makefile-uninstall-rule \
		%{_sysconfdir}/gconf/schemas/%{name}.schemas >/dev/null || :
fi

%post
export GCONF_CONFIG_SOURCE=`gconftool-2 --get-default-source`
gconftool-2 --makefile-install-rule \
	%{_sysconfdir}/gconf/schemas/%{name}.schemas > /dev/null || :
/sbin/ldconfig

%preun
if [ "$1" -eq 0 ]; then
	export GCONF_CONFIG_SOURCE=`gconftool-2 --get-default-source`
	gconftool-2 --makefile-uninstall-rule \
		%{_sysconfdir}/gconf/schemas/%{name}.schemas > /dev/null || :
fi

%postun -p /sbin/ldconfig

%files -f %{name}.lang
%defattr(-,root,root,-)
# Only the following binaries is under GPLv2. Other
# parts are under GPLv2+.
%{_bindir}/evolution-import-rss
%{_sysconfdir}/gconf/schemas/%{name}.schemas
%{_datadir}/evolution/*/errors/org-gnome-evolution-rss.error
%{_datadir}/evolution/*/ui/*.ui
%{_datadir}/evolution/*/images/*.png
%{_libdir}/evolution/*/plugins/org-gnome-evolution-rss.eplug
%{_libdir}/evolution/*/plugins/org-gnome-evolution-rss.xml
%{_libdir}/evolution/*/plugins/liborg-gnome-evolution-rss.so
%{_libdir}/bonobo/servers/GNOME_Evolution_RSS_*.server
%doc AUTHORS
%doc COPYING
%doc ChangeLog
%doc FAQ
%doc NEWS
%doc README
%doc TODO

%changelog
* Wed Jul  2 2008 Lucian Langa <lucilanga@gnome.org> - 0.1.0-1
- update to 0.1.0 release

* Sat Mar  1 2008 Lucian Langa <lucilanga@gnome.org> - 0.0.8-1
- Misc cleanup

* Sat Feb 16 2008 Lucian Langa <cooly@gnome.eu.org> - 0.0.7-6
- Misc cleanup
- Drop gecko requirements till xulrunner is fixed

* Tue Feb 12 2008 Lucian Langa <lucilanga@gnome.org> - 0.0.7-5
- buildroot fixes

* Wed Feb 06 2008 Lucian Langa <lucilanga@gnome.org> - 0.0.7-4
- Modified firefox-devel requirement for build

* Wed Jan 30 2008 Lucian Langa <lucilanga@gnome.org> - 0.0.7-1
- Updates and sanitize

* Thu Jan 24 2008 Lucian Langa <lucilanga@gnome.org> - 0.0.7-1
- Fixed rpmlint warnings
- Updated to Fedora Packaging Guidelines

* Thu Nov 22 2007 Lucian Langa <lucilanga@gnome.org> - 0.0.6-1
- Added gconf schemas
- Added evolution-import-rss

* Tue Sep 04 2007 Lucian Langa <lucilanga@gnome.org> - 0.0.5
- Updated installed files

* Mon Apr 23 2007 root <root@mayday>
- Initial spec file created by autospec ver. 0.8 with rpm 3 compatibility

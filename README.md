# Ev2W
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<strong>Gnome EVOLUTION email client and calendar ported to MSYS2/MinGW64 using GTK.</strong><br><br>
Update and refresh of original work courtesy Novell, Inc., The GNOME Foundation, SUSE and Debian Linux groups, the MSYS2 team and others. <a href="https://en.wikipedia.org/wiki/GNOME_Evolution" target="_blank">Evolution</a> usage is granted under the GNU Lesser General Public License (LGPL). <br><br>
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<strong>What is working?</strong> User interface, GCONF, dll plugins, most protocols like SMTP, IMAP with CRAM-MD5 over TLS 1.x, SSLv3 is also enabled but is not advised for security reasons. Implicit TLS (not requiring STARTTLS) has been successfully tested with Outlook.com on port 993 but needs to be configured as "SSL". This is just how the interface was, SSL really meant any implicit encryption on port 993. Like older versions of Outlook, "TLS" specifically refers to the STARTTLS re-handshake negotiation from port 143. HTML images over HTTPS load, CalDAV and LDAP(S) connectors also work.<br>
<strong>What is not working yet?</strong> GTK2 printing (see <a href="https://github.com/msys2/MINGW-packages/issues/14787">bug</a>).<br><br>
<strong>How does it work?</strong><br>This project is not yet "average-user ready". Ev2W needs to be compiled and installed from source code. For now, Ev2W functions from within an existing <a href="https://stackoverflow.com/questions/30069830/how-can-i-install-mingw-w64-and-msys2">MinGW64</a> directory structure, with executable and dlls in 'bin', configuration in 'etc', assets in 'share', etc. So you will need to install core GTK2-related packages from the MSYS2 project first. Use the MinGW64 releases, NOT the MinGW32 or MSYS2. See 'asctime/MINGW-2023-01-13' fork for MinGW code freeze and organizing PRs back to the MSYS2 team. You will also need to clone, build and install all of the dependency libraries under "asctime/Ev2W-Depends". We have carefully organised example GNU Makefile structures in each subfolder of each related Ev2W project, in a 'mingw64' sub-sub-folder. You are free to use these directly, or simply as a guide in rolling your own. Constructive feedback is welcome. Evolution 2.32 REQUIRES a <a href="//github.com/asctime/Ev2W-Depends">running DBUS daemon</a> and GCONF-2 installation on Windows, which provides compile-time dependencies as well runtime inter-process communication and configuration. Bonobo/CORBA is not required for compiling Ev2W.<br><br>
<strong>What is the goal?</strong><br>Getting the existing "last-known-to-be-working" Windows codebase fully operational under MinGW64 would be a start. The experience would probably be very useful in launching a MinGW64 release of Evolution 3.x upstream, GTK3-based, improved security with all GCONF dependencies re-pointed to the Windows native registry. Project is LGPL, so there is no obvious benefit to directly support LLVM toolchains. Nor do we have a requirement for a 32-bit version (might change).
=======
<strong>Ev2W (Evolution 2.x email address book and calendar client, for Windows) based on MSYS2/MinGW64 and GTK2</strong><br><br>
=======
<strong>Ev2W Mail (Evolution 2.x email address book and calendar client, for Windows) based on MSYS2/MinGW64 and GTK2</strong><br><br>
>>>>>>> 2b37fee... Update README.md
=======
<strong>Gnome EVOLUTION Mail and Calendar client ported to MSYS2/MinGW64 using GTK.</strong><br><br>
>>>>>>> df4c53b... Update README.md
=======
<strong>Gnome EVOLUTION email client and calendar ported to MSYS2/MinGW64 using GTK.</strong><br><br>
>>>>>>> 936e0d0... Update README.md
Update and refresh of original work courtesy Novell, Inc., The GNOME Foundation, SUSE and Debian Linux groups, the MSYS2 team and others. <a href="https://en.wikipedia.org/wiki/GNOME_Evolution" target="_blank">Evolution</a> usage is granted under the GNU Lesser General Public License (LGPL). <br><br>
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<strong>What is working?</strong> User interface, GCONF, dll plugins, several protocols like IMAP and SMTP, CalDAV, LDAP/LDAPS and more.<br>
<strong>What is not working yet?</strong> Some encryption protocols, some message image loading, GTK2 printing (see <a href="https://github.com/msys2/MINGW-packages/issues/14787">bug</a>), binary release, proper instructions and documentation.. <br><br>
<strong>How does it work?</strong><br>This project is not yet "average-user ready". Ev2W needs to be compiled and installed from source code. For now, Ev2W functions from within an existing <a href="https://stackoverflow.com/questions/30069830/how-can-i-install-mingw-w64-and-msys2">MinGW64</a> directory structure, with executable and dlls in 'bin', configuration in 'etc', assets in 'share', etc. So you will need to install core GTK2-related packages from the MSYS2 project first. Use the MinGW64 releases, NOT the MinGW32 or MSYS2. See 'asctime/MINGW-2023-01-13' fork for MinGW code freeze and organizing PRs back to the MSYS2 team. You will also need to clone, build and install all of the dependency libraries under "asctime/Ev2W-Depends" and "asctime/Gnome-2". We have carefully organised example GNU Makefile structures in each subfolder of each related Ev2W project, in a 'mingw64' sub-sub-folder. You are free to use these directly, or simply as a guide in rolling your own. Constructive feedback is welcome. Evolution 2.32 REQUIRES a <a href="//github.com/asctime/Ev2W-Depends">running DBUS daemon</a> and GCONF-2 installation on Windows, which provides compile-time dependencies as well runtime inter-process communication and configuration. Bonobo/CORBA is still required for compiling one or two dependencies but not for actual Evolution functionality in this version.<br><br>
<<<<<<< HEAD
<strong>What is the goal?</strong><br>Getting the existing "last-known-to-be-working" Windows codebase fully operational under MinGW64 would be a start. The experience would probably be very useful in launching a MinGW64 release of Evolution 3.x upstream, GTK3-based, improved security with all GCONF dependencies re-pointed to the Windows native registry and any Gnome references re-pointed to Windows themes - possibly eliminate Gnome desktop library dependencies for Ev2W entirely (example, see evolution-2.32.3\plugins\mail-notification\mail-notification.c). Possible MinGW64 PKGBUILD compilation and installation depending on level of interest from MSYS2 team. As the project is LGPL, there is no obvious benefit to directly support LLVM toolchains, nor will we have the bandwidth to do a 32-bit version (might change).
>>>>>>> 776dbe9... Update README.md
=======
<strong>What is the goal?</strong><br>Getting the existing "last-known-to-be-working" Windows codebase fully operational under MinGW64 would be a start. The experience would probably be very useful in launching a MinGW64 release of Evolution 3.x upstream, GTK3-based, improved security with all GCONF dependencies re-pointed to the Windows native registry and any Gnome references re-pointed to Windows themes - possibly eliminate Gnome desktop library dependencies for Ev2W entirely (example, see evolution-2.32.3\plugins\mail-notification\mail-notification.c). Project is LGPL, so there is no obvious benefit to directly support LLVM toolchains. Nor do we have a requirement for a 32-bit version (might change).
>>>>>>> 8ffd70b... Update README.md
=======
<strong>What is working?</strong> User interface, GCONF, dll plugins, most protocols like IMAP and SMTP, CalDAV, LDAP and SSL.<br>
=======
<strong>What is working?</strong> User interface, GCONF, dll plugins, most protocols like IMAP and SMTP, CalDAV, LDAP and CRAM-MD5 over SSL.<br>
>>>>>>> 53fd45e... Update README.md
=======
<strong>What is working?</strong> User interface, GCONF, dll plugins, most protocols like IMAP and SMTP, CalDAV, LDAP with CRAM-MD5 over SSL.<br>
>>>>>>> fed24a1... Update README.md
=======
<strong>What is working?</strong> User interface, GCONF, dll plugins, most protocols like SMTP, IMAP with CRAM-MD5 over SSL, CalDAV and LDAP(S).<br>
>>>>>>> 165ba73... Update README.md
=======
<strong>What is working?</strong> User interface, GCONF, dll plugins, most protocols like SMTP, IMAP with CRAM-MD5 over SSL (although more secure ssh tunneling <a href="https://github.com/asctime/Ev2W/issues/4">is advised</a>), CalDAV and LDAP(S).<br>
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
>>>>>>> 95d82fb... Update README.md
<strong>What is not working yet?</strong> Some message image loading, GTK2 printing (see <a href="https://github.com/msys2/MINGW-packages/issues/14787">bug</a>), binary release, proper instructions and documentation.. <br><br>
=======
<strong>What is not working yet?</strong> Some message image loading, GTK2 printing (see <a href="https://github.com/msys2/MINGW-packages/issues/14787">bug</a>). <br><br>
>>>>>>> 95873f2... Update README.md
=======
<strong>What is not working yet?</strong> Some message image loading, GTK2 printing (see <a href="https://github.com/msys2/MINGW-packages/issues/14787">bug</a>). Due to security vulnerabilities, a binary release for general use will not be published.<br><br>
<<<<<<< HEAD
>>>>>>> 309fa92... Update README.md
<strong>How does it work?</strong><br>This project is not yet "average-user ready". Ev2W needs to be compiled and installed from source code. For now, Ev2W functions from within an existing <a href="https://stackoverflow.com/questions/30069830/how-can-i-install-mingw-w64-and-msys2">MinGW64</a> directory structure, with executable and dlls in 'bin', configuration in 'etc', assets in 'share', etc. So you will need to install core GTK2-related packages from the MSYS2 project first. Use the MinGW64 releases, NOT the MinGW32 or MSYS2. See 'asctime/MINGW-2023-01-13' fork for MinGW code freeze and organizing PRs back to the MSYS2 team. You will also need to clone, build and install all of the dependency libraries under "asctime/Ev2W-Depends". We have carefully organised example GNU Makefile structures in each subfolder of each related Ev2W project, in a 'mingw64' sub-sub-folder. You are free to use these directly, or simply as a guide in rolling your own. Constructive feedback is welcome. Evolution 2.32 REQUIRES a <a href="//github.com/asctime/Ev2W-Depends">running DBUS daemon</a> and GCONF-2 installation on Windows, which provides compile-time dependencies as well runtime inter-process communication and configuration. Bonobo/CORBA is still required for compiling one or two dependencies but not for actual Evolution functionality in this version.<br><br>
=======
=======
<strong>What is not working yet?</strong> <a href="https://github.com/asctime/Ev2W/issues/4">TLS1.2</a> and most image loading over modern HTTPS, GTK2 printing (see <a href="https://github.com/msys2/MINGW-packages/issues/14787">bug</a>). Due to security vulnerabilities, a binary release for general use will not be published.<br><br>
>>>>>>> 81ca717... Update README.md
=======
<strong>What is working?</strong> User interface, GCONF, dll plugins, most protocols like SMTP, IMAP with CRAM-MD5 over SSL3 (although more secure ssh tunneling <a href="https://github.com/asctime/Ev2W/issues/4">is advised</a>), HTML images over HTTPS, CalDAV and LDAP(S).<br>
<<<<<<< HEAD
<strong>What is not working yet?</strong> IMAP <a href="https://github.com/asctime/Ev2W/issues/4">TLS1.2 encryption</a>, GTK2 printing (see <a href="https://github.com/msys2/MINGW-packages/issues/14787">bug</a>). Due to security vulnerabilities, a binary release for general use will not yet be published.<br><br>
>>>>>>> eb895ad... Update README.md
=======
=======
<strong>What is working?</strong> User interface, GCONF, dll plugins, most protocols like SMTP, IMAP with CRAM-MD5 over SSL3 (more secure ssh tunneling <a href="https://github.com/asctime/Ev2W/issues/4">is advised</a>), HTML images over HTTPS, CalDAV and LDAP(S).<br>
<<<<<<< HEAD
>>>>>>> dc29437... Update README.md
<strong>What is not working yet?</strong> GTK2 printing (see <a href="https://github.com/msys2/MINGW-packages/issues/14787">bug</a>). Due to security vulnerabilities, a binary release for general use will not yet be published.<br>
=======
<strong>What is not working yet?</strong> GTK2 printing (see <a href="https://github.com/msys2/MINGW-packages/issues/14787">bug</a>). Due to <a href="https://en.wikipedia.org/wiki/POODLE">POODLE</a> vulnerability and limitation connecting to modern IMAP providers, a binary release for general use will not yet be published.<br>
>>>>>>> 0e3985d... POODLE link added to Wikipedia
<strong>What could work?</strong> IMAP <a href="https://github.com/asctime/Ev2W/issues/4">TLS1.2 encryption</a>. A new feature, as it was <a href="https://bugzilla.redhat.com/show_bug.cgi?id=1153052">never implemented</a> in Evolution 2.3.<br><br>
>>>>>>> 621ed74... HTML images over HTTPS fixed, IMAP TLS 1.2 as new feature
=======
<strong>What is working?</strong> User interface, GCONF, dll plugins, most protocols like SMTP, IMAP with CRAM-MD5 over TLS 1.x, SSLv3 is also enabled but is not advised for security reasons. Implicit TLS (not requiring STARTTLS) has been successfully tested with Outlook.com on port 993 but needs to be configured as "SSL". This is just how the interface was, it really means any implicit encryption on port 993. Likewise older versions of Outlook, TLS specifically refers to the STARTTLS re-handshake negotiation from port 143. HTML images over HTTPS load, CalDAV and LDAP(S) connectors also work.<br>
=======
<strong>What is working?</strong> User interface, GCONF, dll plugins, most protocols like SMTP, IMAP with CRAM-MD5 over TLS 1.x, SSLv3 is also enabled but is not advised for security reasons. Implicit TLS (not requiring STARTTLS) has been successfully tested with Outlook.com on port 993 but needs to be configured as "SSL". This is just how the interface was, it really means any implicit encryption on port 993. Like older versions of Outlook, TLS specifically refers to the STARTTLS re-handshake negotiation from port 143. HTML images over HTTPS load, CalDAV and LDAP(S) connectors also work.<br>
>>>>>>> 1ea8d7a... Update README.md
=======
<strong>What is working?</strong> User interface, GCONF, dll plugins, most protocols like SMTP, IMAP with CRAM-MD5 over TLS 1.x, SSLv3 is also enabled but is not advised for security reasons. Implicit TLS (not requiring STARTTLS) has been successfully tested with Outlook.com on port 993 but needs to be configured as "SSL". This is just how the interface was, SSL really means any implicit encryption on port 993. Like older versions of Outlook, TLS specifically refers to the STARTTLS re-handshake negotiation from port 143. HTML images over HTTPS load, CalDAV and LDAP(S) connectors also work.<br>
>>>>>>> 12a1687... Update README.md
=======
<strong>What is working?</strong> User interface, GCONF, dll plugins, most protocols like SMTP, IMAP with CRAM-MD5 over TLS 1.x, SSLv3 is also enabled but is not advised for security reasons. Implicit TLS (not requiring STARTTLS) has been successfully tested with Outlook.com on port 993 but needs to be configured as "SSL". This is just how the interface was, SSL really meant any implicit encryption on port 993. Like older versions of Outlook, TLS specifically refers to the STARTTLS re-handshake negotiation from port 143. HTML images over HTTPS load, CalDAV and LDAP(S) connectors also work.<br>
>>>>>>> 87dd0e3... Update README.md
=======
<strong>What is working?</strong> User interface, GCONF, dll plugins, most protocols like SMTP, IMAP with CRAM-MD5 over TLS 1.x, SSLv3 is also enabled but is not advised for security reasons. Implicit TLS (not requiring STARTTLS) has been successfully tested with Outlook.com on port 993 but needs to be configured as "SSL". This is just how the interface was, SSL really meant any implicit encryption on port 993. Like older versions of Outlook, "TLS" specifically refers to the STARTTLS re-handshake negotiation from port 143. HTML images over HTTPS load, CalDAV and LDAP(S) connectors also work.<br>
>>>>>>> 7811461... Update README.md
<strong>What is not working yet?</strong> GTK2 printing (see <a href="https://github.com/msys2/MINGW-packages/issues/14787">bug</a>).<br><br>
>>>>>>> 9270cb9... TLS 1.2 support added
<strong>How does it work?</strong><br>This project is not yet "average-user ready". Ev2W needs to be compiled and installed from source code. For now, Ev2W functions from within an existing <a href="https://stackoverflow.com/questions/30069830/how-can-i-install-mingw-w64-and-msys2">MinGW64</a> directory structure, with executable and dlls in 'bin', configuration in 'etc', assets in 'share', etc. So you will need to install core GTK2-related packages from the MSYS2 project first. Use the MinGW64 releases, NOT the MinGW32 or MSYS2. See 'asctime/MINGW-2023-01-13' fork for MinGW code freeze and organizing PRs back to the MSYS2 team. You will also need to clone, build and install all of the dependency libraries under "asctime/Ev2W-Depends". We have carefully organised example GNU Makefile structures in each subfolder of each related Ev2W project, in a 'mingw64' sub-sub-folder. You are free to use these directly, or simply as a guide in rolling your own. Constructive feedback is welcome. Evolution 2.32 REQUIRES a <a href="//github.com/asctime/Ev2W-Depends">running DBUS daemon</a> and GCONF-2 installation on Windows, which provides compile-time dependencies as well runtime inter-process communication and configuration. Bonobo/CORBA is not required for compiling Ev2W.<br><br>
>>>>>>> c735ac9... Updated Bonobo statement since all gnome2 code is removed.
<strong>What is the goal?</strong><br>Getting the existing "last-known-to-be-working" Windows codebase fully operational under MinGW64 would be a start. The experience would probably be very useful in launching a MinGW64 release of Evolution 3.x upstream, GTK3-based, improved security with all GCONF dependencies re-pointed to the Windows native registry. Project is LGPL, so there is no obvious benefit to directly support LLVM toolchains. Nor do we have a requirement for a 32-bit version (might change).
>>>>>>> 50837cc... Update README.md
<br><br><br>
![Evolution email client for WIndows 10](https://user-images.githubusercontent.com/41893923/213171323-8d0b8c3c-e5af-405a-a05e-b67ab8565024.png)

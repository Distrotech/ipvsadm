%define prefix   /usr
%define ipvs_ver 0.9.13-2.2.15


Summary: Utility to administer the Linux Virtual Server
Name: ipvsadm
Version: 1.10
Release: 1
Copyright: GNU General Public Licence
URL: http://www.linuxvirtualserver.org/
Packager: Horms <horms@vergenet.net>
Group: Applications/System
Source0: http://www.linuxvirutalserver.org/ipvs-%{ipvs_ver}.tar.gz
BuildRoot: /var/tmp/%name-%{PACKAGE_VERSION}-root
Docdir: %{prefix}/doc
Provides: %{name}-%{version}


%description
ipvsadm is a utility to administer the IP virtual server services
offered by the Linux kernel with virtual server patch.


%prep
%setup -n ipvs-%{ipvs_ver}


%build

cd ipvsadm
CFLAGS="${RPM_OPT_FLAGS}" make


%install
rm -rf $RPM_BUILD_ROOT
mkdir -p ${RPM_BUILD_ROOT}/{sbin,usr/man/man8}
cd ipvsadm
BUILD_ROOT=${RPM_BUILD_ROOT} make install

#File finding code thanks to Samuel Flory of VA Linux Systems
cd ${RPM_BUILD_ROOT}
# Directories
find . -type d | sed '1,2d;s,^\.,\%attr(-\,root\,root) \%dir ,' \
  > ${RPM_BUILD_DIR}/ipvs-%{version}-%{release}.files
# Files
find . -type f | sed 's,^\.,\%attr(-\,root\,root) ,' \
  >> ${RPM_BUILD_DIR}/ipvs-%{version}-%{release}.files
# Symbolic links
find . -type l | sed 's,^\.,\%attr(-\,root\,root) ,' \
  >> ${RPM_BUILD_DIR}/ipvs-%{version}-%{release}.files


%clean
rm -rf $RPM_BUILD_DIR/ipvs-0.9.9-2.2.14
rm -rf $RPM_BUILD_ROOT
rm ${RPM_BUILD_DIR}/ipvs-%{version}-%{release}.files


%post

%postun

%preun

%files -f ../ipvs-%{version}-%{release}.files
%doc ipvsadm/README


%changelog
* Mon Apr 10 2000 Horms <horms@vergenet.net>
- created for version 1.9

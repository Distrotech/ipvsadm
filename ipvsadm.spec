%define prefix   /usr
%define ipvs_ver 0.9.10-2.2.14


Summary: Utility to administer the Linux Virtual Server
Name: ipvsadm
Version: 1.9
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

#Funky NPROC code to speed things up courtesy of Red Hat's kernel rpm
if [ -x /usr/bin/getconf ] ; then
    NRPROC=$(/usr/bin/getconf _NPROCESSORS_ONLN)
    if [ $NRPROC -eq 0 ] ; then
        NRPROC=1
    fi
else
    NRPROC=1
fi
NRPROC=`expr $NRPROC + $NRPROC`

cd ipvsadm
CFLAGS="${RPM_OPT_FLAGS}" make -j $NRPROC


%install
rm -rf $RPM_BUILD_ROOT
mkdir -p ${RPM_BUILD_ROOT}/{sbin,usr/man/man8}
cd ipvsadm
BUILD_ROOT=${RPM_BUILD_ROOT} make install

#File finding code thanks to Samuel Flory of VA Linux Systems
cd ${RPM_BUILD_ROOT}
# Directories
find . -type d | sed '1,2d;s,^\.,\%attr(-\,root\,root) \%dir ,' \
  > ${RPM_BUILD_DIR}/ipvs-%{ver}-%{rel}.files
# Files
find . -type f | sed 's,^\.,\%attr(-\,root\,root) ,' \
  >> ${RPM_BUILD_DIR}/ipvs-%{ver}-%{rel}.files
# Symbolic links
find . -type l | sed 's,^\.,\%attr(-\,root\,root) ,' \
  >> ${RPM_BUILD_DIR}/ipvs-%{ver}-%{rel}.files


%clean
rm -rf $RPM_BUILD_DIR/ipvs-0.9.9-2.2.14
rm -rf $RPM_BUILD_ROOT
rm ${RPM_BUILD_DIR}/ipvs-%{ver}-%{rel}.files


%post

%postun

%preun

%doc ipvsadm/README
%files -f ../ipvs-%{ver}-%{rel}.files


%changelog
* Mon Apr 10 2000 Horms <horms@vergenet.net>
- created for version 1.9

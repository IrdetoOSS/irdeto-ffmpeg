################################################################################
# File:     irdeto-xps-export.spec
# Brief:    Irdeto xps data export interface
# Author:   Yu Duan (yu.duan@irdeto.com)
# License:  Irdeto B.V. Proprietary license
################################################################################

%define package_name    irdeto-xps-export
%define package_version %{VERSION}
%define package_release %{RELEASE}

################################################################################
# Main package for irdeto-xps-export
################################################################################
Summary:        Irdeto xps data export interface
Name:           %{package_name}
Version:        %{package_version}
Release:        %{package_release}%{?dist}
Group:          Development/Libraries
License:        Irdeto B.V. Proprietary License
BuildArch:      x86_64
Vendor:         Irdeto B.V.
Packager:       Yu Duan (yu.duan@irdeto.com)
Source0:        %{package_name}.tar.gz

BuildRequires:  gcc
BuildRequires:  cmake3

%if "%{?dist}" == ".el7"
BuildRequires:  irdeto-x265-devel
BuildRequires:  irdeto-x265
%endif

%description
Irdeto unified interface between decoder and encoder

%prep
mkdir -p %{package_name}
tar -xf %{_sourcedir}/%{package_name}.tar.gz -C %{package_name}

%build
cd %{package_name}
%cmake3 -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_LIBDIR=lib64 -DCMAKE_SKIP_RPATH:BOOL=OFF -DSYS=%{?dist} .
make %{?_smp_mflags}
cd -

%install
install -d %{buildroot}/%{_docdir}/%{package_name}-%{package_version}
cd %{package_name}
make install DESTDIR=%{buildroot}
cd -

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%package        devel
Summary:        Irdeto xps interface development files
Group:          Development/Libraries
Version:        %{package_version}
Release:        %{package_release}%{?dist}
License:        Irdeto B.V. Proprietary license
Vendor:         Irdeto B.V
Packager:       Maksym Koshel (maksym.koshel@irdeto.com)

Requires:       %{package_name} = %{package_version}-%{package_release}%{?dist}

%description    devel
Irdeto xps interface development files

%files devel
%defattr(-,root,root)
%{_includedir}/irxps*
%{_libdir}/cmake*

%files
%defattr(-,root,root)
%{_libdir}/irxps*

%changelog

* Fri Dec 01 2017 Yu Duan <yu.duan@irdeto.com> 7.0.0-0
- Removed the exporting of reference frames for HEVC, reference frames will be managed at application side for HEVC

* Wed Nov 15 2017 Maksym Koshel <maksym.koshel@irdeto.com> 6.0.0-0
- Upgraded to new FFmpeg version 3.4

* Mon Jul 31 2017 Yu Duan <yu.duan@irdeto.com> 5.0.0-0
- Now exporting of reference frame in separate header
- New function is shared by two codecs AVC and HEVC
- New function for exporting ref frames should be called at application side

* Wed Jun 07 2017 Maksym Koshel <maksym.koshel@irdeto.com> 4.0.1-0
- Added support of Centos 6.x

* Fri May 19 2017 Yu Duan <yu.duan@irdeto.com> 4.0.0-0
- Aligned with latest HEVC clean user story, removed lots of unused export variables
- Splited import hevc function into meta data and reference frame, they will be called from x265 instead of embedder

* Wed May 17 2017 Yu Duan <yu.duan@irdeto.com> 3.0.0-0
- Changed implementation to header only project with small shared library which handles memory management
- Added validation of library context to handle unintialized field and memory corruption from ffmpeg

* Mon Mar 27 2017 Yu Duan <yu.duan@irdeto.com> 2.0.0-0
- Made this project as shared library

* Thu Mar 23 2017 Yu Duan <yu.duan@irdeto.com> 2.0.0-0
- Added paramter input_size to allow user specify the range of valid data that needs
  to be exported
* Thu Mar 16 2017 Yu Duan <yu.duan@irdeto.com> 1.2.0-0
- Added mode parameters, currently 3 modes are supported: raw, base64 and lz4

* Thu Mar 16 2017 Yu Duan <yu.duan@irdeto.com> 1.1.0-0
- Reimplemented this library based on libb64

* Wed Mar 15 2017 Yu Duan <yu.duan@irdeto.com> 1.0.0-0
- Reimplemented this library based on glib base64

* Tue Mar 14 2017 Yu Duan <yu.duan@irdeto.com> 0.1.0-0
- Put implementation in separate source file cause library is big and we have
  multiple definitions plus performance issues

* Fri Mar 10 2017 Yu Duan <yu.duan@irdeto.com> 0.0.9-0
- Initial implementaion of irdeto-export-codec helper functions

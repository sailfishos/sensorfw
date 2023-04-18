Name:       sensorfw-qt5
Summary:    Sensor Framework Qt5
Version:    0.14.3
Release:    0
License:    LGPLv2+
URL:        https://github.com/sailfishos/sensorfw
Source0:    %{name}-%{version}.tar.bz2
Source1:    sensorfwd.service
Source2:    sensorfw-qt5-hybris.inc
Requires:   sensord-configs
Requires:   systemd
Requires(preun): systemd
Requires(post): /sbin/ldconfig
Requires(post): systemd
Requires(postun): /sbin/ldconfig
Requires(postun): systemd
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Test)
BuildRequires:  pkgconfig(mlite5)
BuildRequires:  pkgconfig(libsystemd)
BuildRequires:  pkgconfig(ssu-sysinfo)
BuildRequires:  doxygen
BuildRequires:  pkgconfig(libudev)
Provides:   sensord-qt5
Obsoletes:   sensorframework

%description
Sensor Framework provides an interface to hardware sensor drivers through logical sensors. This package contains sensor framework daemon and required libraries.


%package devel
Summary:    Sensor framework daemon libraries development headers
Requires:   %{name} = %{version}-%{release}
Requires:   pkgconfig(Qt5Core)
Requires:   pkgconfig(Qt5DBus)
Requires:   pkgconfig(Qt5Network)

%description devel
Development headers for sensor framework daemon and libraries.


%package tests
Summary:    Unit test cases for sensord
Requires:   %{name} = %{version}-%{release}
Requires:   pkgconfig(Qt5Test)
Requires:   testrunner-lite
Requires:   python(abi) > 3.0
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description tests
Contains unit test cases for CI environment.


%package configs
Summary:    Sensorfw configuration files
BuildArch:  noarch
Requires:   %{name} = %{version}
Provides:   sensord-configs
Provides:   config-n900
Provides:   config-aava
Provides:   config-icdk
Provides:   config-ncdk
Provides:   config-oemtablet
Provides:   config-oaktraili
Provides:   config-u8500

%description configs
Sensorfw configuration files.


%package doc
Summary:    API documentation for libsensord
BuildArch:  noarch
Requires:   %{name} = %{version}-%{release}
Requires:   doxygen
Obsoletes:  %{name}-docs

%description doc
API documentation for libsensord
 Doxygen-generated API documentation for sensord.


%prep
%autosetup -n %{name}-%{version}

%build
# setup proper lib
sed "s=@LIB@=%{_lib}=g" sensord-qt5.pc.in > sensord-qt5.pc
sed "s=@LIBDIR@=%{_libdir}=g" tests/tests.xml.in > tests/tests.xml
unset LD_AS_NEEDED
export LD_RUN_PATH=%{_libdir}/sensord-qt5/

%qmake5  \
    CONFIG+=ssusysinfo\
    CONFIG+=mce\
    PC_VERSION=`echo %{version} | sed 's/+.*//'`

%make_build

%install
%qmake5_install

install -D -m644 %{SOURCE1} $RPM_BUILD_ROOT/%{_unitdir}/sensorfwd.service

mkdir -p %{buildroot}/%{_unitdir}/graphical.target.wants
ln -s ../sensorfwd.service %{buildroot}/%{_unitdir}/graphical.target.wants/sensorfwd.service

%preun
if [ "$1" -eq 0 ]; then
systemctl stop sensorfwd.service || :
fi

%post
/sbin/ldconfig
systemctl daemon-reload || :
systemctl reload-or-try-restart sensorfwd.service || :

%postun
/sbin/ldconfig
systemctl daemon-reload || :

%post tests -p /sbin/ldconfig

%postun tests -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%license COPYING
%{_libdir}/libsensorclient-qt5.so.*
%{_libdir}/libsensordatatypes-qt5.so.*
%attr(755,root,root)%{_sbindir}/sensorfwd
%dir %{_libdir}/sensord-qt5
%{_libdir}/sensord-qt5/*.so
%{_libdir}/libsensorfw*.so.*

%config %{_sysconfdir}/dbus-1/system.d/sensorfw.conf
%dir %{_sysconfdir}/sensorfw
%{_unitdir}/sensorfwd.service
%{_unitdir}/graphical.target.wants/sensorfwd.service

%files devel
%defattr(-,root,root,-)
%{_libdir}/libsensorfw*.so
%{_libdir}/libsensordatatypes*.so
%{_libdir}/libsensorclient*.so
%{_libdir}/pkgconfig/*
%{_includedir}/sensord-qt5/*
%{_datadir}/qt5/mkspecs/features/sensord.prf

%files tests
%defattr(-,root,root,-)
%{_libdir}/libsensorfakeopen*.so
%{_libdir}/libsensorfakeopen*.so.*
%dir %{_libdir}/sensord-qt5/testing
%{_libdir}/sensord-qt5/testing/*
%dir %{_datadir}/sensorfw-tests
%attr(755,root,root)%{_datadir}/sensorfw-tests/*.p*
%attr(644,root,root)%{_datadir}/sensorfw-tests/*.xml
%attr(644,root,root)%{_datadir}/sensorfw-tests/*.conf
%attr(755,root,root)%{_bindir}/datafaker-qt5
%attr(755,root,root)%{_bindir}/sensoradaptors-test
%attr(755,root,root)%{_bindir}/sensorapi-test
%attr(755,root,root)%{_bindir}/sensorbenchmark-test
%attr(755,root,root)%{_bindir}/sensorchains-test
%attr(755,root,root)%{_bindir}/sensordataflow-test
%attr(755,root,root)%{_bindir}/sensord-deadclient
%attr(755,root,root)%{_bindir}/sensordiverter.sh
%attr(755,root,root)%{_bindir}/sensordriverpoll-test
%attr(755,root,root)%{_bindir}/sensordummyclient-qt5
#%attr(755,root,root)%{_bindir}/sensorexternal-test
%attr(755,root,root)%{_bindir}/sensorfilters-test
%attr(755,root,root)%{_bindir}/sensormetadata-test
%attr(755,root,root)%{_bindir}/sensorpowermanagement-test
%attr(755,root,root)%{_bindir}/sensorstandbyoverride-test
%attr(755,root,root)%{_bindir}/sensortestapp

%files configs
%defattr(-,root,root,-)
%config %{_sysconfdir}/sensorfw/sensord.conf.d/*conf

%files doc
%{_prefix}/share/doc/sensord-qt5

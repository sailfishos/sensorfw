Name: hybris-libsensorfw-qt5-binder
Version:    0.14.2
Release:    0
Provides: hybris-libsensorfw-qt5 = %{version}-%{release}
Conflicts: hybris-libsensorfw-qt5 <= 0.10.9
Conflicts: hybris-libsensorfw-qt5-hal
Obsoletes: hybris-libsensorfw-qt5 < %{version}-%{release}

BuildRequires: pkgconfig(libgbinder) >= 1.1.14

# additional parameters for qmake
%define qmake_extra_parameters CONFIG+=binder

%include rpm/sensorfw-qt5-hybris.inc

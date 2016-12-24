@ECHO OFF
IF "%PLATFORM%" == "x64" (
	cmake . -G "Visual Studio 14 2015 Win64" -DUSE_QT4=OFF -DUSE_QT5=ON -DCMAKE_PREFIX_PATH="C:/Qt/5.7/msvc2015_64"
) ELSE (
	cmake . -G "Visual Studio 14 2015" -DUSE_QT4=OFF -DUSE_QT5=ON -DCMAKE_PREFIX_PATH="C:/Qt/5.7/msvc2015"
)
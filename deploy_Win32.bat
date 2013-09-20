msbuild pom.targets /t:Construct /p:PackagePlatform=Win32
msbuild pom.targets /t:Deploy /p:PackagePlatform=Win32

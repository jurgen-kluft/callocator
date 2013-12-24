msbuild pom.targets /t:Construct /p:PackagePlatform=x64 /p:PackageIDE=VS2012 /p:PackageToolSet=v110
msbuild pom.targets /t:Install /p:PackagePlatform=x64 /p:PackageIDE=VS2012 /p:PackageToolSet=v110

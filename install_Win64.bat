msbuild pom.targets /t:Construct /p:PackagePlatform=x64 /p:PackageIDE=VS2010 /p:PackageToolSet=v100
msbuild pom.targets /t:Install /p:PackagePlatform=x64 /p:PackageIDE=VS2010 /p:PackageToolSet=v100

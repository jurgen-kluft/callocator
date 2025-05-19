package callocator

import (
	cbase "github.com/jurgen-kluft/cbase/package"
	"github.com/jurgen-kluft/ccode/denv"
	cunittest "github.com/jurgen-kluft/cunittest/package"
)

// GetPackage returns the package object of 'callocator'
func GetPackage() *denv.Package {
	// Dependencies
	cunittestpkg := cunittest.GetPackage()
	cbasepkg := cbase.GetPackage()

	// The main (callocator) package
	mainpkg := denv.NewPackage("callocator")
	mainpkg.AddPackage(cunittestpkg)
	mainpkg.AddPackage(cbasepkg)

	// 'callocator' library
	mainlib := denv.SetupCppLibProject("callocator", "github.com\\jurgen-kluft\\callocator")
	mainlib.AddDependencies(cbasepkg.GetMainLib()...)

	// 'callocator' unittest project
	maintest := denv.SetupDefaultCppTestProject("callocator"+"_test", "github.com\\jurgen-kluft\\callocator")
	maintest.AddDependencies(cunittestpkg.GetMainLib()...)
	maintest.Dependencies = append(maintest.Dependencies, mainlib)

	mainpkg.AddMainLib(mainlib)
	mainpkg.AddUnittest(maintest)
	return mainpkg
}

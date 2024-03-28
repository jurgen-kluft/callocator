package callocator

import (
	cbase "github.com/jurgen-kluft/cbase/package"
	"github.com/jurgen-kluft/ccode/denv"
	ccore "github.com/jurgen-kluft/ccore/package"
	cunittest "github.com/jurgen-kluft/cunittest/package"
)

// GetPackage returns the package object of 'callocator'
func GetPackage() *denv.Package {
	// Dependencies
	cunittestpkg := cunittest.GetPackage()
	cbasepkg := cbase.GetPackage()
	ccorepkg := ccore.GetPackage()

	// The main (callocator) package
	mainpkg := denv.NewPackage("callocator")
	mainpkg.AddPackage(cunittestpkg)
	mainpkg.AddPackage(cbasepkg)
	mainpkg.AddPackage(ccorepkg)

	// 'callocator' library
	mainlib := denv.SetupDefaultCppLibProject("callocator", "github.com\\jurgen-kluft\\callocator")
	mainlib.Dependencies = append(mainlib.Dependencies, ccorepkg.GetMainLib())
	mainlib.Dependencies = append(mainlib.Dependencies, cbasepkg.GetMainLib())

	// 'callocator' unittest project
	maintest := denv.SetupDefaultCppTestProject("callocator_test", "github.com\\jurgen-kluft\\callocator")
	maintest.Dependencies = append(maintest.Dependencies, cunittestpkg.GetMainLib())
	maintest.Dependencies = append(maintest.Dependencies, ccorepkg.GetMainLib())
	maintest.Dependencies = append(maintest.Dependencies, cbasepkg.GetMainLib())
	maintest.Dependencies = append(maintest.Dependencies, mainlib)

	mainpkg.AddMainLib(mainlib)
	mainpkg.AddUnittest(maintest)
	return mainpkg
}

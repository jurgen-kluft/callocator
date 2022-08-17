package main

import (
	cpkg "github.com/jurgen-kluft/callocator/package"
	ccode "github.com/jurgen-kluft/ccode"
)

func main() {
	ccode.Generate(cpkg.GetPackage())
}

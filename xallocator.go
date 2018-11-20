package main

import (
	"github.com/jurgen-kluft/xallocator/package"
	"github.com/jurgen-kluft/xcode"
)

func main() {
	xcode.Init()
	xcode.Generate(xallocator.GetPackage())
}

package main

import (
	"github.com/jurgen-kluft/xcode"
	"github.com/jurgen-kluft/xallocator/package"
)

func main() {
	xcode.Generate(xallocator.GetPackage())
}

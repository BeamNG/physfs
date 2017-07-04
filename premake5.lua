project "physfs"
  kind "StaticLib"
  --compilationunitenabled(true) -- make it use compilation units for faster compilation
  buildoptions {
    '/wd4090', -- disables warning C4090: 'function': different 'const' qualifiers
  }
  defines {
    '_CRT_SECURE_NO_WARNINGS',
    'PHYSFS_PLATFORM_WINDOWS',
  }
  includedirs  {
    'src',
  }
  files {
    "src/*.h",
    "src/*.c",
    "src/aes/*.h",
    "src/aes/*.c",
  }
  removefiles {
    '**lzma**',
  }

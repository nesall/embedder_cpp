echo Building embedder_cpp release version...
mkdir build_rel
cd build_rel
cmake .. -DCMAKE_BUILD_TYPE=Releas
cmake --build . --config Release
cd ..
echo Build complete!
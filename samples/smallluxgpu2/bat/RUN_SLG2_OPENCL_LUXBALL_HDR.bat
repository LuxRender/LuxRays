@echo off
SLG2.exe -D renderengine.type 4 -D opencl.cpu.use 1 -D opencl.gpu.use 1 -D screen.refresh.interval 50 -D path.sampler.type INLINED_RANDOM scenes\luxball\render-fast-hdr.cfg
pause

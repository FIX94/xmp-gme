fork of mudlords xmp-gme plugin with the following changes:  
-forked last github GME version from kode54, edited to use cubic interpolation in .spc files  
-fixed last track in for example .nsf files not playing right  
-added in stereo effect when supported like .nsf files  
-added in 5 second fade-out for tracks that dont have default length like .nsf files  
-added in title/length updates between each subsong so info from for example .nfse files gets shown  
-set samplerate default to 48khz  
to compile yourself:  
check out this project to C:\xmp-gme, then check out my File_Extractor and Game_Music_Emu forks and put them into C:\xmp-gme\xmp-gme, then build xmp-gme.sln  
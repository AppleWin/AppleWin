bin_to_c.exe bootsector.bin > acme.txt
bin_to_c.exe bootsector     > merlin.txt
git diff --no-index           merlin.txt acme.txt

.PHONY : all hax firm0 firm1 sector screen_init stage2 installer clean

TARGET		=	arm9loaderhax
PYTHON 		=	python
INDIR		=	data_input
OUTDIR		=	data_output

all : $(OUTDIR) hax installer

hax : $(OUTDIR) firm0 firm1 sector screen_init stage2

$(OUTDIR):
	@[ -d $(OUTDIR) ] || mkdir -p $(OUTDIR)

firm0:
	@cd payload_stage1 && make
	@cp $(INDIR)/new3ds90.firm $(OUTDIR)/firm0.bin
	@dd if=payload_stage1/payload_stage1.bin of=$(OUTDIR)/firm0.bin bs=512 seek=1922 conv=notrunc
	@echo FIRM0 done!

firm1:
	@cp $(INDIR)/new3ds10.firm $(OUTDIR)/firm1.bin
	@echo FIRM1 done!

sector:
	@$(PYTHON) common/sector_generator.py $(INDIR)/secret_sector.bin $(INDIR)/otp.bin $(OUTDIR)/sector.bin
	@echo SECTOR done!

screen_init:
	@[ -d payload_stage2/data ] || mkdir -p payload_stage2/data
	$(MAKE) -C screen_init
	@cp screen_init/screen_init.bin payload_stage2/data/

stage2:
	@cp screen_init/screen_init.bin payload_stage2/data
	@$(MAKE) -C payload_stage2
	@cp payload_stage2/payload_stage2.bin $(OUTDIR)/stage0x5C000.bin

installer:
	@cd payload_installer && make
	@echo INSTALLER done!

clean:
	@echo clean...
	@cd payload_stage1 && make clean
	$(MAKE) -C screen_init clean
	@cd payload_stage2 && make clean
	@cd payload_installer && make clean TARGET=../$(TARGET)

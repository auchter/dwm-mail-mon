
dwm-mail-mon: dwm-mail-mon.c
	gcc --std=gnu99 -Wall -lX11 $< -o $@

.PHONY: clean
clean:
	rm -f dwm-mail-mon


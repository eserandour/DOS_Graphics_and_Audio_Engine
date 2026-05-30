wcc -ml -0 -os -I. main.c > main.out
wcc -ml -0 -os -I. timer.c > timer.out
wcc -ml -0 -os -I. keyboard.c > keyboard.out
wcc -ml -0 -os -I. video.c > video.out
wcc -ml -0 -os -I. palette.c > palette.out
wcc -ml -0 -os -I. graphics.c > graphics.out
wcc -ml -0 -os -I. font1.c > font1.out
wcc -ml -0 -os -I. font1dat.c > font1dat.out
wcc -ml -0 -os -I. image.c > image.out
wcc -ml -0 -os -I. font2.c > font2.out
wcc -ml -0 -os -I. scene.c > scene.out
wcc -ml -0 -os -I. trans.c > trans.out
wcc -ml -0 -os -I. scenes\scene1.c > scene1.out
wcc -ml -0 -os -I. scenes\scene2.c > scene2.out
wcc -ml -0 -os -I. scenes\scene3.c > scene3.out
wcc -ml -0 -os -I. scenes\scene4.c > scene4.out
wcc -ml -0 -os -I. scenes\scene5.c > scene5.out
wlink @LINK.RSP > link.out

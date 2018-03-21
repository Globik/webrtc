# webrtc
It's time to hack Lorenzo's code in Janus webrtc gateway server written in C

A standalone dedicated WebRTC gateway server is cool, but a standalone WebRTC library even better.

Сервер это хорошо, а библиотека еще лучше.
Ибо нет ничего лучше, чем свой сигнальный протокол под свою платформу и под свой вебсервер.

Yeah, today I got it! 
Proof of concept.
Today is a great day. I'm lucky to get to becoming friends among kore.c - as a transport layer for Janus webrtc gateway - and Janus itself.
I got it. Russia, Chelyabinsk-city, 19.03.2018.

One more time I'm saying: a standalone webrtc gateway server is good, but a relatively "small" gateway library is even better.
Theoretically Janus's code could one to add to any good web framework. Today I've proofed it.
 I just did it and here you are!

It's will be good for a small projects. I ain't saying about over 1Mio users of project. I'm saying about 100 users. 
It's enough to start to make money.
It's C. It's fast enough and not so monstruose and slowly like existent node.js  parts webrtc solutions(based on native webrtc and family).

# What I removed from janus core

I removed admin api, identification api, statistic api, transport layer plugins. That all will be in a kore API part, on its own.

# in a kore_janus folder

## kore web framework

[kore](https://github.com/jorisvink/kore)

## janus webrtc gateway server

[janus](https://github.com/meetecho/janus-gateway)

Just compiled it as libjanus.so. And linked to the kore api. Based(hardcoded) on a libjanus_echotest.so plugin.

Run:
```
	# kodev run
```

### Test:

Open a browser that does websockets, surf to https://127.0.0.1:8888
or whatever configured IP you have in the config.
Hit the connect button to open a websocket session.
And see WebRTC DataChannel API in action. Based on libjanus_echotest.so plugin.

	
In plans to do the same with a nice [mediasoup.js](https://github.com/versatica/mediasoup) 
webrtc streaming  solution - server side.
It's based on a uv.c loop system.
# How Can I Contribute to HiBy Modding?
Just chatting in the discord about issues that you have, and talking with the people who are developing, is really helpful. Getting feedback and discussing with the community is so helpful for the developers.

And talking about mods with people on Reddit, where relevant, will help to broaden our community.

## I'm a Dev, What Can I Do?
There's a huge variety of ways that devs can help out.

### If you don't know C
It's not hard to learn a new language (in my experience). So I'd say, why not learn C today?

However, there is actually quite a bit of stuff that you can do without ever looking at C.
**Developing Mods**: 
- Without knowing C, you'll have a hard time modifying the `hiby_player` binary, but that's only one tiny part of modding.
- You can poke around the rootfs files, looking at config files to see what functionality you could change/add in order to improve something.
	- For example, `Car Mode` is a setting that's not accessible by default. But by setting `car_mode` to `1` instead of `0` in `usr/resource/set_functions.json`, it becomes visible in the settings.
- You can make custom themes
	- In `/usr/resource/litegui` there's theme folders.
	- By looking through these files and making changes, you can develop a custom theme.

**Contributing to Tools and Documentation**:
- Tools and documentation are what makes modding these devices possible.
- We document everythign we can about how the device works, how the original firmware works, what changing certain values does.
- We have build tools that make it easy to unpack and repack firmware, to help make mods.
- The tools/scripts were probably just written by a single person.
	- So by looking at them, you provide an additional set of eyes that'll see problems the other person missed.
	- Also the tools are usually very unpolished. Improving the user experience (UX) means that more people will be willing/able to help with the project. So helping to improve the UX with the tools is a huge help
- We document as we go, but there's always room for more documentation.
	- Look through the instructions that are written, and if something is confusing, then just talk in the community about that and propose a way to make it more clear/complete.
	- If you find documentation/information that's missing, adding it will help make stuff easier in the future.
    	- For example, writing out something that explains what Ingenic is and what it has to do with HiBy's DAPs.


### If You Do Know C
(TODO)

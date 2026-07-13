# How Can I Contribute to HiBy Modding?
Just chatting in the discord about issues that you have, and talking with the people who are developing, is really helpful. Getting feedback and discussing with the community is so helpful for the developers.

And talking about mods with people on Reddit, where relevant, will help to broaden our community.

## I'm a Dev, What Can I Do?
There's a huge variety of ways that devs can help out. Look through the [TODOs](TODO.md) and see if there's any you want to tackle.

> *Also, if there's things in this repository that you want to work on, add it to the TODOs, or make an Issue, or make a Discussion post to talk about your goal. Then make a branch/fork to implement your change, and make a Pull Request to have your contribution merged in.*

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
If you do know C, you can still, of course, do all of the things said above.

But you can also help with some of the deeper modding projects. There's several project where people are writing new programs to run on the HiBy OS devices. There's Rockbox, open_hiby_player, and spotui-hiby-r3proii to name a couple. Contributing to these projects and/or starting your own would help greatly with getting the perfect experience on these devices.

You can also help us learn more about the inner workings of the original firmware by using decompilation tools on the original `hiby_player` binary.
- For example, we found what each of the values of `Output Port Switch` in ALSA correlate to. This helps us know better how to switch output modes.

Goal: modify wled baseline code to build a custom bin file for me to upload to my lolin-s2-mini

Requirements:

Phase 1 (remove splash page)
- Remove the initial welcome screen (go to the controls, wifi settings) and force the user directly to the Controls page.

Phase 2 (change navigation)
- Remove all navigation options except the power and config button.
- Move any removed naviation options to the Config page
    - Add new pill button links in different colour
    - Links I'm expecting to see: Time, Sync, Peek, Info, Colours, Effects, Segments, Presets

Phase 4 (home page changes)
- Add a small alert panel at the top saying 'Limited functionality in demo model'
- Add a smaller colour wheel
- Add a brightness slider
- Add text at the bottom of the page
    - Configured by Liquid Light Design (heart icon)
    - Hyperlink the heart icon to the Config page
#        - Pull the heart icon from www.liquidlightdesign.com
#        - We will use this heart icon as pixel tracking
#        - Add query id onto end of url to include DPdemo as an identifier

Bottom nav bar: 
1 - Remove Segments, Preset buttons
2 - Replace colours icon (pallate) with text reading ‘Colours’. Keep the hyperlink the same.
3 - Replace effects icon (face) with text reading ‘Effects’. Keep the hyperlink the same.
4 - Text must be white.
5 - Space the text equally across the nav bar | <---> Colours  <---> Effects  <---> |


Remove functionality to allow user to side swipe on mobile to get to the next window.

1 - remove build time. you never go this right.
2- Add a small alert panel under the top nav bar saying 'Limited app functionality in demo model' Make the background #eb8634 with white text.
3 - 




Phase 5
- Apply logic whenever a device disconnects from the wifi to always revert back to z_cycle_preset option.


Rules:
- Make changes to the mobile view only.
- This is an existing working codebase and it can already be uploaded to my device and it works.
- You must not make changes to the core workings of the code
- Most of my changes are cosmetic
- Where possible put configurations into a centralised file for easy changes by me in the future
- Document all requirements, business rules as you go.
- Update your memory after every commit or half hour so that you can load it into context when our next session starts.


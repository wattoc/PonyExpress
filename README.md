![Pony Express Icon](images/pony_express_icon_64.png)**PonyExpress** for [Haiku](https://www.haiku-os.org/).

A native Dropbox client for Haiku.  It currently uses libcurl for the heavy lifting but is otherwise a pure Haiku app written in C/C++.

This is very alpha software - use at your own risk...  I will not be held responsible if some valuable documents get lost.  There are likely leaks and segfaults, etc. within.

For safeties sake, consider this a magic trash can in that your documents are binned as soon as you drag them in.  If you can't handle that, don't use it.

This software bundles in my Dropbox App key for PonyExpress - if you fork or otherwise adapt the Dropbox handling code from this software, please use your own Dropbox App key instead.  Feel free to borrow the code otherwise, and improvements are most welcome.

### Features

* Automatic syncing of files and folders between a Haiku Dropbox folder and your linked Dropbox account
* PKCE authentication, proper use of tokens, refresh, etc.
* Uses the Dropbox V2 API
* Basic Haiku integration, notifications, etc.

### Deskbar Icon

![Screenshot](images/deskbar_icon.png)

PonyExpress runs in the background and can be configured by clicking on the Deskbar Icon and selecting the Configure option.

### Configuration Window

Here you can set up your account on Dropbox

![Screenshot](images/configuration_window.png)

- Click on the Request Code button

- Your browser should open up to a link to Dropbox for PonyExpress.  You may need to sign in to your Dropbox account.

- If you authorise PonyExpress, you'll be given some text to paste in.  Paste this in the Authorization Code field.

- Close the window and the syncing should commence.

### Stuff to do
- Some tidying and refactoring
- Allow zipped downloads to improve speed
- Handle operations against folders better (eg. currently deletes each file in a folder, then the folder which isn't optimal.)
- Allow resuming uploads/downloads
- Record queues on exit so pending activity can be resumed generally
- Handle collisions/sync issues
- Potentially support for other cloud syncing services, should their APIs be easy enough to work with and support PKCE with OAUTH or something similar
- Various bugs I haven't discovered yet

### Stuff I probably won't do
- All the extended stuff, Paper, Teams, Requests, etc. support.  I only care about conveniently sharing files between Haiku and my myriad other desktops

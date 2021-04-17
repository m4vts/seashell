# seashell
A custom shell by Onur Mavitaş and Bulut Boru. This project was carried out as part of the Operating Systems class at Koc University.

Seashell is a program that provides interface to the operating system by gathering input from the user and executing programs based on that input. It uses execv() system call to execute common Linux programs such as ls, mkdir, cp, mv, date, and gcc.

Seashell has a custom "shortdir" command. The purpose is to reach the directory a short name instead of typing the whole path. There are supportive options for shortdir: "shortdir set <name>" associates <name> with the current directory. "shortdir jump <name>" changes to the directory associated with <name>. "shortdir del <name>" deletes the name-directory association. "shortdir clear" deletes all the name-directory associations. "shortdir list" lists all the name-directory associations. This command lives across shell sessions.

"highlight is another custom seashell command. Highlight command takes a word-color pair and a text file as an  input. For each instance of the word appearing in the text file, the command prints the line where the word appears and highlights the word with that color. The colors can be one of the r (red), g (green), or b (blue) colors.

<img width="923" alt="Screen Shot 2021-04-17 at 20 28 55" src="https://user-images.githubusercontent.com/51910678/115121472-999f8080-9fbb-11eb-8d61-9fdb13a54c35.png">


"goodMorning" is another custom seashell command. This command takes a time specification and a music file as arguments and set an alarm to wake you up by playing the music using rhythmbox.

<img width="494" alt="Screen Shot 2021-04-17 at 20 17 25" src="https://user-images.githubusercontent.com/51910678/115121424-64932e00-9fbb-11eb-9f81-f13ca0e6845e.png">


"kdiff" is another command of seashell that can be used to compare two files in given paths. It has two modes. -a: the utility reads the input files as text files and compares them line-by-line. It prints the differing lines from each file and then prints the count of differing lines. -b: the utility reads the input files as binary files and compares them byte-by-byte. If two files are different, the utility prints a message about how many bytes are different between the files.


<img width="401" alt="Screen Shot 2021-04-17 at 20 17 35" src="https://user-images.githubusercontent.com/51910678/115121271-61e40900-9fba-11eb-8262-c42538b586bd.png">


"siri" is another command that takes a questions to get a weather report and start the chronometer.

<img width="896" alt="Screen Shot 2021-04-17 at 20 24 50" src="https://user-images.githubusercontent.com/51910678/115121372-141bd080-9fbb-11eb-85e4-a0dc83568e0e.png">

<img width="890" alt="Screen Shot 2021-04-17 at 20 25 27" src="https://user-images.githubusercontent.com/51910678/115121391-2990fa80-9fbb-11eb-9215-80f91555f02f.png">


 



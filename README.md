Name: Kirill Slabun

Date: 04/12/2026

Environment: VS Code, and MobaTextEditor

How to compile the project: Type 'make'

Example of how to run the project: ./oss -n 40 -s 18 -t 2 -i 0.1 -f log.txt

Generative AI used: gemini-3-flash-preview

Reason for not using AI: For this project, I wanted to experiment with using AI from the terminal rather than a browser or software. To use AI through the terminal on Windows, I used WSL to run Linux Ubuntu in PowerShell. Then, I ran npm install -g @google/gemini-cli to install Google Gemini CLI. This opened another level of possibilities for me when it comes to using AI for code writing. Gemini had direct access to the files in the folder when I launched Gemini (via the gemini command). It has access to all sorts of tools, such as Edit, FindFiles, GoogleSearch, ReadFile, ReadFolder, ReadManyFiles, SaveMemory, SearchText, Shell, WebFetch, and WriteFile. These tools allowed Gemini to execute commands and edit files. This removed all the monotonous work of copying responses from the browser to VS Code. This also removed the need to constantly update AI with new information when I make changes to the files. I make changes to the files in VS Code, save them, and Gemini can read the latest versions. Gemini can also create a GEMINI.md file with an overview and all the necessary information about the project, so I do not need to repeat myself in the prompts.

Using Gemini this way was a very pleasant experience, except for parts where it oversimplified the code and deleted non-critical parts that were still important. This is exactly why I wanted to use AI for this project: to experiment and find better ways to use this technology.

Prompts: 

/init

Can you change oss.c and worker.c files to satisfy requirements documented in the cs4760Assignment5Spring2026.pdf. For now, make changes to implement resource management (requesting and releasing resources by child processes). Don’t do deadlock detection or the termination of deadlocked child processes.

Install cc

For starters, just run the code with one child process.

Now, could you implement deadlock detection and termination of one of the deadlocked processes? Use example code in deadlockdetection.cpp, deadlockdetection.h, ddtest.cpp, and makefile-2 located in the ‘Deadlock Detection Example’ folder for deadlock detection implementation.
I have also made some changes to the code after you have worked on it. It mostly consists of non-critical changes. Organized OSS, improved readability, and added comments. Changed how information is displayed and logged to a file. Reorganized the worker code to enhance readability and incorporated comprehensive comments. Implemented additional functionality to display worker information upon both initialization and termination. I would like to keep the code readable and easy to understand through comments. The outputs were changed for easier debugging, so keep them as well if possible. I have also added if statements to check for errors when initializing shared memory and related resources. Keep them as well.

Summary: Gemini gave really good answers to the requests; there were some unpolished edges. For example, it removed proper logging and comments that were previously in place, made worker logic slightly incorrect, did not implement proper population and cleaning of the PCB table, and did not fully implement the final report. However, there were no major bugs, aside from the worker logic being slightly incorrect, and I fixed all the leftover imperfections.

GitHub: https://github.com/SKirill98/os-project-5

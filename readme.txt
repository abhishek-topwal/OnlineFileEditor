ASSIGNMENT 3 - ONLINE MULTI CLIENT-SERVER FILE EDITOR

NAME - ABHISHEK TOPWAL
ID   - 21CS60R01

INSTRUCTIONS TO RUN CODE:

1. The file server.c and client.c should be present in different folders.

2. SERVER should start executing first
	COMMAND TO RUN SERVER: gcc server.c -o sr
			       ./sr

	COMMAND TO RUN CLIENT: gcc client.c -o cl
			       ./cl


GENERAL DESIGN DECISIONS:

1. Please keep the files to be uploaded in the same folder as the client.c folder.

2. The client 5-digit ID is displayed as the client gets connected to the server.


COMMANDS SPECIFIC DESIGN DECISIONS:

1. INVITE commmand and its response:
	
	-- After a collaboration invite has been sent by say C1(12345) to C2(12346) for file f1.txt as Editor.

	-- C2 will receive this request as:
		Invitation from client 12345 to join in file || f1.txt || as Editor

	-- After this another message will pop as:
		TO ACCEPT TYPE: /YES <sender_client_ID> (without <>)
		TO REJECT TYPE: /NO <sender_client_ID> (without <>)

	-- The client C2 is supposed to response in the above format only, else it will keep asking for the response in the above format.
		e.g. /YES 12345
		e.g. /NO 12345

2. Upload file 

	-- The uploaded file has to be viewed at the folder containing server.c .

3. Download file

	-- The file will be downloaded at folder containing client.c 
	
	-- The client ID will be appended to the filename.
	   e.g. Client C1(12345) request-  /download f1.txt
	        If C1 is authorized to download, the file will be downloaded as 12345_f1.txt


4. Delete file contents.

	-- If client is authorized to delete file contents, the changes has to be viewed at the folder containing server.c

	-- If whole file is not deleted, the remaining contents will be sent back to the client.


5. Insert into file

	-- to insert multiple lines input, client should use \n inside the quotes while writing at the terminal. This will be treated as new line by the server.

 
//........................................................THANK YOU.................................................................//


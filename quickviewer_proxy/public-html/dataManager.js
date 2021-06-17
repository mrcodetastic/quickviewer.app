var KEY_PKT_START = new Uint8Array([49,49,49,49]); // Ascii symbol number 1 x 4 (reciept of packets only
var KEY_CON_ACK = new Uint8Array([67,79,78,78]); //ascii: "CONN"

var KEY_GET_IMAGE = new Uint8Array([71,73,77,71]); 			//ascii: "GIMG"
var KEY_SET_CURSOR_POS = new Uint8Array([83,67,85,80]); 	//SCUP
var KEY_SET_CURSOR_DELTA = new Uint8Array([83,67,85,68]); 	//SCUD
var KEY_SET_MOUSE_KEY = new Uint8Array([83,77,75,83]); 		//SMKS
var KEY_SET_MOUSE_WHEEL = new Uint8Array([83,77,87,72]); 	//SMWH
var KEY_SET_KEY_STATE = new Uint8Array([83,75,83,84]); 		//"SKST";
var KEY_CHANGE_DISPLAY = new Uint8Array([67,72,68,80]); 	//"CHDP";
var KEY_REFRESH_DISPLAY = new Uint8Array([82,69,70,72]); 	//"REFH";
var KEY_TILE_RECEIVED = new Uint8Array([84,76,82,68]); 		//"TLRD";
var KEY_SET_AUTH_REQUEST = new Uint8Array([83,65,82,81]); 	//"SARQ";
var KEY_CONNECT_UUID = new Uint8Array([67,84,85,85]); 		//"CTUU";
var KEY_DEBUG = new Uint8Array([68,66,85,71]); 				//"DBUG";

var KEY_IMAGE_PARAM = "73,77,71,80";	//new Uint8Array([73,77,71,80]); //ascii: "IMGP";
var KEY_IMAGE_TILE = "73,77,71,84";		//IMGT
var KEY_IMAGE_SCREEN = "73,77,71,83";	//IMGS
var KEY_SET_NONCE = "83,84,78,67";		//STNC
var KEY_SET_AUTH_RESPONSE 		= "83,65,82,80"; //SARP;
var KEY_CHECK_AUTH_RESPONSE 	= "67,65,82,80"; //CARP;
var KEY_SET_NAME 				= "83,84,78,77"; //STNM;

var HEADER_SIZE 	 = 4;
var COMMAND_SIZE 	 = 4;
var REQUEST_MIN_SIZE = HEADER_SIZE + COMMAND_SIZE; // header ('1111') + command

var MAX_PAYLOAD_SIZE = 256000;

/*
 *  https://stackoverflow.com/questions/50257210/convert-qbytearray-from-big-endian-to-little-endian/50257560
 *  The concept of endianness only applies to multi-byte data types. So the "right" order depends on what the data type is.
 *  But if you just want to reverse the array, I'd use std::reverse from the C++ algorithms library. - bnaecker May 9 '18 at 15:54
 */


class DataManager
{
    constructor()
    {
        this.isConnected = false;
        this.isSessionStarted = false;
        //payload = new Uint8Array([]); // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Typed_arrays
        this.loginClass 	= null;
        this.displayField 	= null;
        this.extraKeys 		= null;
        this.messageField 	= null;
		
		this.session_uuid   = null;
		this.session_nonce  = null;
		this.partner_id     = null;
		this.partner_pw     = null;
		
		this.dataTmp 			= new Uint8Array();
		

        // console.log("performed construction.");
    }
	
	// Initiated by login
	startLogin(partner_id, partner_pw)
	{
		this.partner_id = partner_id;
		this.partner_pw = partner_pw;
		
		this.initDataManager();
	}
    
    initDataManager()
    {
        this.startSocket();
        this.startXmlHttpRequest();
    }
    
    startSocket()
    {
		console.log("StartSocket()");
        // ServerHTTP_Class
		
		var location_str = String(window.location.href);
		
		if (location_str.includes("app"))
		{
			this.webSocket = new WebSocket('wss://' + window.location.hostname + ':8443/');
		} else {
			this.webSocket = new WebSocket('ws://' + window.location.hostname + ':8080/');			
		}

        if(!this.webSocket)
        {
            this.showDisconnectMessage();
            return;
        }

        this.webSocket.binaryType = 'arraybuffer';
        this.webSocket.onopen = this.socketConnected.bind(this);
        this.webSocket.onmessage = this.setData.bind(this);
        this.webSocket.onclose = this.showDisconnectMessage.bind(this);
    }
    
    startSession()
    {
		console.log("startSession()");			

		this.session_uuid =  uuidv4();
        if(this.webSocket)
        {
            if(this.webSocket.readyState === WebSocket.OPEN)
            {
                this.isConnected = true;
				console.log("isConnected = true");						
                this.removeDisconnectMessage();
				
				if ( !this.isSessionStarted &&  this.partner_id != null &&  this.partner_pw != null) 
				{
					// kick off the workflow, witha conn request, server will respond if it's valid & setup direct proxy fwd
					this.sendToSocket("1111CONN|" + this.partner_id + "|1|");
				//	this.sendToSocket("1111CONN|2142717624|1|");	
				}
					
            }
            else
            {
                this.showDisconnectMessage();
                // console.log("try 'startSession' but socket is not connected yet");
            }
        }
    }
    
    socketConnected()
    {
		console.log("socketConnected()");		
        setTimeout(this.startSession.bind(this),1500);
    }
    
    setData(event)
    {
		//console.log("setData(event)");
		
		/*
        // https://www.html5rocks.com/en/tutorials/webgl/typed_arrays/#toc-dataview
        var rawdata = new Uint8Array(event.data);

        var header       = rawdata.subarray(0, COMMAND_SIZE).toString(); // bytes 0-3

        if ( header ==  KEY_PKT_START ) {
            // console.log ("Starting new payload.");

            var command      = rawdata.subarray(4, 4+COMMAND_SIZE); // bytes 4-7
            //// console.log(command);
			//   // console.log("Command is: " + command.toString());

            var payload_size = this.uint32FromArray(rawdata.subarray(8, 8 + 4));  // bytes 8-11
            //this.expectedPayloadSize = payload_size;
            // console.log("expected payload_size is: " + payload_size);

            payload = rawdata.subarray(12); // bytes 12+
            // console.log("filling new data buffer with " +  rawdata.length + " bytes");

			this.newData(command);


        } else {
            // console.log("Failed to recieve packet header!");
        }
		*/		
        var data = event.data;
		
        var dataArray = new Uint8Array(data);
        var activeBuf = new Uint8Array(dataArray.length + this.dataTmp.length);
        activeBuf.set(this.dataTmp, 0); // copy exisitng
        activeBuf.set(dataArray, this.dataTmp.length); // append new data

        var size = activeBuf.length; // unprocessed data + new

        if(size < REQUEST_MIN_SIZE)
            return;
		
	//	while (true)
	//	{
			var header_byte_count = 0, header_end = -1;
			for(var i = 0; i < size-HEADER_SIZE; i++) {		
				if (activeBuf[i] == 49) { header_byte_count++; } // found a 49				
				if ( header_byte_count == 4) {
					header_end = i+1;					
				//	console.log("Packet header ends at byte " + header_end);
					break;
				} // header
			} // for	

			// Process from this point foward
            var command      = activeBuf.slice(header_end, header_end+COMMAND_SIZE); // bytes 4-7
			//console.log("Command is: " + command.toString());

            var payload_size = this.uint32FromArray(activeBuf.slice(header_end+COMMAND_SIZE, header_end+(COMMAND_SIZE*2)));  // bytes 8-11
			
           // console.log("expected payload_size is: " + payload_size);
	       // console.log("actual data size is: " + size);
			//console.log(activeBuf);
			
			if (payload_size > size-(header_end+(COMMAND_SIZE*2))) {
				console.log("Available data less than expected payload size. Not processing");
			}
			else
			{
				var payload = activeBuf.slice(header_end+COMMAND_SIZE*2, header_end+(COMMAND_SIZE*2)+payload_size); // bytes 12+
				// console.log("filling new data buffer with " +  rawdata.length + " bytes");
				this.newData(command, payload);
				this.dataTmp = new Uint8Array(); // empty it out 				
				//break;
			}	
			
	//	} // while	

    } // end
    
    newData(cmd, payload)
    {
        if(cmd.length !== 4)
            return;

        var command = cmd.toString();
        // console.log("newData(): Recieved command: " + command);
		
		if (command == KEY_CON_ACK) {
			var string2 = new TextDecoder().decode(payload);
			console.log("Connection acknowledged for " + string2);			
			
			if (string2.includes(this.partner_id))
			{			
				// Request nonce directly from partner
				var response = new Uint8Array(this.session_uuid.length + 8);
				response.set(KEY_CONNECT_UUID,0);
				response.set(this.arrayFromUint32(this.session_uuid.length), 4);
				response.set(this.session_uuid, 8)

				this.sendToSocket(response);
				//this.sendToSocket("CTUU" +  this.arrayFromUint32(this.session_uuid.length) + this.session_uuid);	
				
			} else {
				if(this.loginClass)
				{
                    this.loginClass.showWrongRequest();
				}
			}
			
		}
		else if(command === KEY_SET_NONCE) // receive nonce from server 
        {
            // console.log("KEY_SET_NONCE");
			/*
            if(this.loginClass)
            {
                this.loginClass.createLoginHtml();
                this.loginClass.setNonce(btoa(String.fromCharCode.apply(null, payload)));
            }
			*/
			
			this.session_nonce = btoa(String.fromCharCode.apply(null, payload));
			
			// send auth request
			var concatFirst = btoa(rstr_md5(this.partner_id + this.partner_pw));
			var concatSecond = btoa(rstr_md5(concatFirst + this.session_nonce));
			var binaryString = atob(concatSecond);

			var response = new Uint8Array(binaryString.length);
			for(var i=0;i<response.length;++i)
				response[i] = binaryString.charCodeAt(i);

			this.sendDataMessage2(KEY_SET_AUTH_REQUEST, response);	// if all goes well we get a good KEY_SET_AUTH_RESPONSE
			
        }
        else if(command === KEY_SET_AUTH_RESPONSE)
        {
            var response = this.uint32FromArray(payload);
            
            if(response === 1)
            {
                if(this.loginClass)
                    this.loginClass.removeLoginHtml();
                
                if(this.displayField)
                    this.displayField.initDisplayField();
                
                if(this.extraKeys)
                    this.extraKeys.createExtraKeysHtml();
                
                this.isSessionStarted = true;
                this.webSocket.send(KEY_GET_IMAGE);
            }
            else
            {
                if(this.loginClass)
                    this.loginClass.showWrongRequest();
                
                this.isSessionStarted = false;
            }
            
        }
        else if(command === KEY_IMAGE_PARAM)
        {
            var imageWidth = this.uint32FromArray(payload.slice(0,4));
            var imageHeight = this.uint32FromArray(payload.slice(4,8));
            var rectWidth = this.uint32FromArray(payload.slice(8,12));
            
            if(this.displayField)
                this.displayField.setImageParameters(imageWidth,imageHeight,rectWidth);
        }
        else if(command === KEY_IMAGE_TILE)
        {
            var posX = this.uint32FromArray(payload.slice(0,4));
            var posY = this.uint32FromArray(payload.slice(4,8));
            var tileNum = this.uint32FromArray(payload.slice(8,12));
            var b64encoded = 'data:image/webp;base64,' + btoa(String.fromCharCode.apply(null, payload.slice(12)));
            
            if(this.displayField)
                this.displayField.setImageData(posX, posY, b64encoded, tileNum);
        }
        else if(command === KEY_IMAGE_SCREEN)
        {
             console.log("got a full screen image.");
             var b64encoded = 'data:image/webp;base64,' + btoa(String.fromCharCode.apply(null, payload));

            if(this.displayField)
                this.displayField.setImageScreenData(b64encoded);
        }
        else if(command === KEY_CHECK_AUTH_RESPONSE)
        {
            var uuid = payload.subarray(0,16);
            var name = payload.subarray(16,payload.length);
            var nameString = String.fromCharCode.apply(null, name);

            if(this.loginClass && !this.isSessionStarted)
                this.loginClass.addDesktopButton(uuid, nameString);
        }
        else if(command === KEY_SET_NAME)
        {
            var titleName = String.fromCharCode.apply(null, payload);

            if(titleName)
                document.title = titleName;
        }
        else {

             console.log("!!!Unknown command received.");
             console.log("newData:",command.toString(),command, payload);
        }
    }
	
	requestRefresh() // clean the local packet cache as well
	{
		this.dataTmp = new Uint8Array(); // empty it out 	
		this.sendToSocket(KEY_REFRESH_DISPLAY);
	}
		
    
    sendToSocket(data)
    {
        if(this.isConnected)
        {
            this.webSocket.binaryType = 'arraybuffer';
            this.webSocket.send(data);
        }
        else
        {
             console.log("try 'sendToSocket' but socket is not connected yet");
        }
    }

    sendTextToSocket(text)
    {
        if(this.isConnected)
        {
            this.webSocket.binaryType = 'blob';
            this.webSocket.send(text);
        }
    }
    
    setDisplayField(dField)
    {
        if(dField)
        {
            this.displayField = dField;
            dField.setDataManager(this);
        }
    }
    
    setLoginClass(lClass)
    {
        if(lClass)
        {
            this.loginClass = lClass;
            lClass.setDataManager(this);
        }
    }
    
    setExtraKeys(eKeys)
    {
        if(eKeys)
        {
            this.extraKeys = eKeys;
            eKeys.setDataManager(this);
        }
    }
	
	
    
    sendInput(key, param1, param2)
    {
        var posSize = this.arrayFromUint16(4);
        var posXBuf = this.arrayFromUint16(param1);
        var posYBuf = this.arrayFromUint16(param2);

        var buf = new Uint8Array(12);
        buf[0] = key[0];
        buf[1] = key[1];
        buf[2] = key[2];
        buf[3] = key[3];
        buf[4] = posSize[0];
        buf[5] = posSize[1];
        buf[6] = 0;		
        buf[7] = 0;				
        buf[8] = posXBuf[0];
        buf[9] = posXBuf[1];
        buf[10] = posYBuf[0];
        buf[11] = posYBuf[1];

        this.sendToSocket(buf);
    }
	
    sendDataMessage2(key, parameter)
    {
        var paramSize =  parameter.length
        var buf = new Uint8Array(paramSize + 8);
        buf[0] = key[0];
        buf[1] = key[1];
        buf[2] = key[2];
        buf[3] = key[3];
        buf[4] = paramSize;
        buf[5] = paramSize >> 8;
        buf[6] = paramSize >> 16;		
        buf[7] = paramSize >> 24;
		
        for(var i=0;i<paramSize;++i)
            buf[i + 8] = parameter[i];

        this.sendToSocket(buf);
    }
	
    /*
    uint16FromArray(buf)
    {
        if(buf.length === 2)
        {
            var number = buf[0] | buf[1] << 8;
            return number;
        }
        else return 0x0000;
    }
	*/
    arrayFromUint16(num)
    {
        var buf = new Uint8Array(2);
        buf[0] = num;
        buf[1] = num >> 8;
        return buf;
    }


    uint32FromArray(buf)
    {
        if(buf.length === 4)
        {
            var number = buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
            return number;
        }
        else return 0x0000;
    }

    arrayFromUint32(num)
    {
        var buf = new Uint8Array(4);
        buf[0] = num;
        buf[1] = num >> 8;
        buf[2] = num >> 16;
        buf[3] = num >> 24;
        return buf;
    }
    
    // ________________ XMLHttpRequest ________________
    startXmlHttpRequest()
    {
        this.xmlHttpRequest = new XMLHttpRequest();
        this.xmlHttpRequest.onload = this.readFromXmlHttpRequest.bind(this);
    }

    readFromXmlHttpRequest()
    {
        var data = this.xmlHttpRequest.responseText;
        var url = this.xmlHttpRequest.responseURL;
        
        if(url.includes('keyboard.html'))
            this.displayField.setKeyboardHtml(data);
    }

    sendToXmlHttpRequest(method, request, data)
    {
        this.xmlHttpRequest.open(method, request);
        this.xmlHttpRequest.send(data);
    }
    
    showDisconnectMessage()
    {

        if(this.messageField)
            return;
        
        this.messageField = document.createElement('div');
        this.messageField.id = 'messageField';
        this.messageField.style.cssText = 'position:absolute;\
            margin:0; \
            padding:0; \
            left:0; \
            top:0; \
            width:100%; \
            height:100%; \
            z-index:10; \
            background: #111;';
        
        this.messageLabel = document.createElement('div');
        this.messageLabel.id = 'messageLabel';
		if (!this.isSessionStarted)
		{
			this.messageLabel.innerHTML = 'Unable to connect to remote computer.';
		} else 
		{
			this.messageLabel.innerHTML = 'Connection to remote computer lost.';			
		}
        this.messageLabel.style.cssText = 'position: absolute; \
            background: none; \
            color: #FFF; \
            font: 16px Arial; \
            text-align: center; \
            line-height: 30px; \
            border: none; \
            top: calc(50% - 15px); \
            left: calc(50% - 125px); \
            width: 300px; \
            height: 30px;';
        this.messageField.append(this.messageLabel);
        
        this.messageImg = document.createElement("img");
        this.messageImg.id = 'messageImg';
        this.messageImg.src = "favicon.ico";
        this.messageImg.style.cssText = 'position: absolute; \
            border: none; \
            top: calc(50% - 15px); \
            left: calc(50% - 160px); \
            width: 30px; \
            height: 30px;';
        this.messageField.append(this.messageImg);

        document.body.append(this.messageField);
		
		// Reset vars
        this.isConnected = false;
        this.isSessionStarted = false;
		this.session_nonce 	= null;
		this.session_uuid 	= null;
		this.partner_id 	= null;
		this.partner_pw 	= null;
				
    }
    
    removeDisconnectMessage()
    {
        if(this.messageField)
        {
            this.messageField.remove;
            this.messageField = null;
        }
    }
    
    
    // ________________________________________________
}

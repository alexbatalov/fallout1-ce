//current command in ascii decimal
let currentcmd = [0,0,0] 
let currentfile = "";
const sleep = ms => new Promise(r => setTimeout(r,ms));
var loaded = false;
Module['print'] = function(text){console.log(text);}
Module['preRun'] = function()
{
    
    function stdin(){return 10};
    var stdout = null;
    function stderr(text){alert("stderr: " + text)} 
    FS.init(stdin,stdout,stderr);
    FS.mount(IDBFS,{},"/home/web_user/");
    
}
Module['noInitialRun'] = true

function keyev(ev) {
        home = "/home/web_user/fallout1/"
        if(ev.key == "`"){
        file_selector.click()
        }
        else if(ev.key == "="){
            FS.syncfs(false,function(){alert("save attempted")});
        }
        else if(ev.key == "\\"){
            readfrom = home.concat('',prompt("Read which directory?"))
            alert(readfrom)
            alert(FS.readdir(readfrom))
            
            
        } else if(ev.key == "]"){
            FS.syncfs(true,function(){
                try {
                    FS.mkdir("/home/web_user/fallout1")
                    FS.mkdir("/home/web_user/fallout1/DATA")
                } catch (error) {
                    
                }
                alert("Data loaded. You may now make changes.")
                loaded = true;
            });
        }
        else if(ev.key == '-'){
            del = home.concat('',prompt("Delete which file?"))
            FS.unlink(del)
        }
    
    
    
}

document.addEventListener('keydown', keyev, true);

document.addEventListener('click', (ev) => {
    console.log("event is captured only once.");
    args = []
    if(!loaded){
    FS.syncfs(true,function(){
        try {
            FS.mkdir("/home/web_user/fallout1/")
            FS.mkdir("/home/web_user/fallout1/DATA")
        } catch (error) {
            
        }
        if(FS.analyzePath("/preload/DATA").exists){
            FS.chdir("/preload")
            FS.symlink("/home/web_user/fallout1/DATA/SAVEGAME","/preload/DATA/SAVEGAME");
        }
        else{
            FS.chdir("/home/web_user/fallout1");
        }
        
        
        document.removeEventListener("keydown",keyev,true);
        document.getElementById("Instructions1").remove();
        document.getElementById("Instructions2").remove();
        document.getElementById("Instructions3").remove();
        document.getElementById("Instructions4").remove();
        document.getElementById("Instructions5").remove();
        document.getElementById("Instructions6").remove();
        Module.callMain(args);
});}
    else{
        FS.chdir("/home/web_user/fallout1");
        document.removeEventListener("keydown",keyev,true);
        document.getElementById("Instructions1").remove();
        document.getElementById("Instructions2").remove();
        document.getElementById("Instructions3").remove();
        document.getElementById("Instructions4").remove();
        document.getElementById("Instructions5").remove();
        document.getElementById("Instructions6").remove();
        Module.callMain(args);
    }
    
  }, { once: true });


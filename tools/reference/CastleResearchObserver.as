package {
    import flash.display.*;
    import flash.events.*;
    import flash.net.Socket;
    import flash.system.Security;
    import flash.utils.*;
    public class CastleResearchObserver extends Sprite {
        private var socket:Socket=new Socket();
        private var clock:Timer=new Timer(220);
        private var rows:Array=[];
        public function CastleResearchObserver() {
            mouseEnabled=mouseChildren=false;
            var args:Object=root.loaderInfo.parameters;
            Security.loadPolicyFile("xmlsocket://127.0.0.1:"+args.tcp);
            socket.addEventListener(Event.CONNECT,function(e:Event):void {
                send("ready");clock.addEventListener(TimerEvent.TIMER,sample);clock.start();
            });
            socket.connect("127.0.0.1",int(args.tcp));
        }
        private function send(header:String):void {socket.writeUTF(header);socket.writeUnsignedInt(0);socket.flush();}
        private function measure(gs:Object):void {
            var team:Object=gs.team;team.gold=2000;team.mana=500;rows=[];
            team.castleDefence.cleanUpUnits();
            for(var tier:int=1;tier<=3;tier++) {
                var tech:int=-27-tier;
                var before:int=team.gold;
                var allowed:Boolean=team.tech.getTechAllowed(tech);
                team.tech.startResearching(tech);
                var ticks:int=0;
                while(!team.tech.isResearched(tech)&&ticks<1000) {team.tech.update(gs.game);++ticks;}
                if(ticks==1000)throw new Error("Castle research did not complete tier "+tier);
                team.castleDefence.update(gs.game);
                var actors:Array=[];
                for each(var u:Object in team.castleDefence.units)
                    actors.push({x:u.px,y:u.py,z:u.pz,scale:u.perspectiveScale,flying:u.flyingHeight,body:u.mc.mc.currentFrame});
                rows.push({tier:tier,allowed:allowed,cost:before-team.gold,ticks:ticks,count:actors.length,actors:actors});
            }
        }
        private function sample(e:Event):void {
            try {
                if(!DisplayObjectContainer(root).numChildren)return;
                var app:Object=DisplayObjectContainer(root).getChildAt(0);
                if(!("currentScreen" in app))return;
                var request:int=socket.bytesAvailable?socket.readUnsignedByte():0;
                if(request==69) {
                    app.campaign.upgradeMap["Castle Archer II"].upgraded=true;
                    app.campaign.upgradeMap["Castle Archer III"].upgraded=true;
                    app.campaign.techAllowed[-29]=1;app.campaign.techAllowed[-30]=1;
                    app.campaign.currentLevel=6;app.campaign.campaignPoints=4;app.campaign.difficultyLevel=1;
                    app.showScreen("campaignGameScreen");return;
                }
                if(app.currentScreen()=="campaignGameScreen") {
                    var gs:Object=app.getScreen(app.currentScreen());gs.doAiUpdates=false;gs.strictPause=true;
                    if(request==82)measure(gs);
                }
                send(JSON.stringify({page:app.currentScreen(),rows:rows}));
            } catch(error:Error) {send("error|"+error.toString());}
        }
    }
}

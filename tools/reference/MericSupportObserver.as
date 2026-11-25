package {
    import flash.display.*;
    import flash.events.*;
    import flash.net.Socket;
    import flash.system.Security;
    import flash.utils.*;
    public class MericSupportObserver extends Sprite {
        private var socket:Socket=new Socket();
        private var clock:Timer=new Timer(220);
        private var rows:Array=[];
        private var phase:String="idle";
        public function MericSupportObserver() {
            mouseEnabled=mouseChildren=false;
            var args:Object=root.loaderInfo.parameters;
            Security.loadPolicyFile("xmlsocket://127.0.0.1:"+args.tcp);
            socket.addEventListener(Event.CONNECT,function(e:Event):void {
                send("ready");clock.addEventListener(TimerEvent.TIMER,sample);clock.start();
            });socket.connect("127.0.0.1",int(args.tcp));
        }
        private function send(header:String):void {socket.writeUTF(header);socket.writeUnsignedInt(0);socket.flush();}
        private function clear(gs:Object):void {
            for each(var team:Object in [gs.team,gs.team.enemyTeam]) {
                for each(var actor:Object in team.units.concat()) {
                    if(actor.isMiner())actor.ai.targetOre=null;
                    team.removeUnitCompletely(actor,gs.game);
                }team.population=0;
            }
            gs.game.spatialHash.clear();
        }
        private function spawn(gs:Object,type:int,x:Number,enemy:Boolean=false):Object {
            var team:Object=enemy?gs.team.enemyTeam:gs.team;
            if(team.unitGroups[type]==null)team.unitGroups[type]=[];
            var u:Object=gs.game.unitFactory.getUnit(type);team.spawn(u,gs.game);
            u.px=x;u.py=125;u.dx=u.dy=0;u.update(gs.game);return u;
        }
        private function command(gs:Object,name:String,x:Number=0,target:int=-1):Object {
            var C:Class=Class(getDefinitionByName("com.brockw.stickwar.engine.Ai.command::"+name));
            var c:Object=new C(gs.game);
            c.goalX=c.realX=x;c.goalY=c.realY=125;c.targetId=target;return c;
        }
        private function hash(gs:Object,a:Object,b:Object):void {
            gs.game.spatialHash.clear();gs.game.spatialHash.add(a);gs.game.spatialHash.add(b);
        }
        private function measure(gs:Object):void {
            rows=[];
            for each(var gap:int in [200,250]) {
                clear(gs);gs.game.frame+=1;
                var u:Object=spawn(gs,3,2400);
                var foe:Object=spawn(gs,8,2400+gap,true);
                u.ai.setCommand(gs.game,command(gs,"AttackMoveCommand",3000));
                hash(gs,u,foe);u.ai.update(gs.game);
                rows.push({name:"support-distance",gap:gap,dx:u.dx,dy:u.dy,busy:u.isBusy(),healToggled:u.isHealToggled,cureToggled:u.isCureToggled});
            }
            clear(gs);gs.game.frame+=1;
            u=spawn(gs,3,2400);var patient:Object=spawn(gs,8,2533);patient.health=100;
            u.ai.setCommand(gs.game,command(gs,"StandCommand"));
            hash(gs,u,patient);u.ai.update(gs.game);
            rows.push({name:"spatial-heal",gap:133,busy:u.isBusy(),healToggled:u.isHealToggled,cureToggled:u.isCureToggled,cooldown:u.healCooldown()});
        }
        private function sample(e:Event):void {
            try {
                if(!DisplayObjectContainer(root).numChildren)return;
                var app:Object=DisplayObjectContainer(root).getChildAt(0);
                if(!("currentScreen" in app))return;
                var request:int=socket.bytesAvailable?socket.readUnsignedByte():0;
                if(request==69) {
                    app.campaign.currentLevel=8;app.campaign.campaignPoints=8;app.campaign.difficultyLevel=1;
                    app.showScreen("campaignGameScreen");return;
                }
                if(app.currentScreen()=="campaignGameScreen") {
                    var gs:Object=app.getScreen(app.currentScreen());gs.doAiUpdates=false;gs.strictPause=true;
                    if(request==82)measure(gs);
                }
                send(JSON.stringify({page:app.currentScreen(),rows:rows}));
            }catch(error:Error){send("error|"+phase+"|"+error.toString());}
        }
    }
}

package {
    import flash.display.*;
    import flash.events.*;
    import flash.net.Socket;
    import flash.system.Security;
    import flash.utils.*;
    public class TodoOrdersObserver extends Sprite {
        private var socket:Socket=new Socket();
        private var clock:Timer=new Timer(220);
        private var rows:Array=[];
        private var phase:String="idle";
        public function TodoOrdersObserver() {
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
            for each(var type:int in [19,9]) {
                phase="giant "+type;clear(gs);
                var giant:Object=spawn(gs,type,2400);
                var target:Object=spawn(gs,8,2500,true);
                giant.forceFaceDirection(-1);
                giant.ai.setCommand(gs.game,command(gs,"HoldCommand"));
                giant.ai.setCommand(gs.game,command(gs,"MoveCommand",2500,target.id));
                rows.push({name:"giant-direct-target",type:type,mayAttack:giant.ai.mayAttack,command:giant.ai.currentCommand.type});
            }
            phase="Meric move";clear(gs);
            var monk:Object=spawn(gs,3,2400);
            var patient:Object=spawn(gs,8,2440);
            patient.health=100;hash(gs,monk,patient);
            monk.ai.setCommand(gs.game,command(gs,"MoveCommand",2700));
            monk.ai.update(gs.game);monk.update(gs.game);
            rows.push({name:"meric-move",busy:monk.isBusy(),cooldown:monk.healCooldown(),pose:monk.mc.currentLabel});
            phase="Meric cast";monk.ai.setCommand(gs.game,command(gs,"StandCommand"));
            hash(gs,monk,patient);monk.ai.update(gs.game);monk.update(gs.game);
            for(var i:int=0;i<8;i++)monk.update(gs.game);
            var before:int=monk.mc.mc.currentFrame;
            monk.ai.setCommand(gs.game,command(gs,"MoveCommand",2700));
            var kept:Boolean=monk.mc.currentLabel=="attack_1"&&monk.mc.mc.currentFrame==before;
            for(i=0;i<130;i++) {
                hash(gs,monk,patient);monk.ai.update(gs.game);monk.update(gs.game);patient.update(gs.game);
            }
            rows.push({name:"meric-cast-then-move",keptAnimation:kept,busy:monk.isBusy(),x:monk.px,patientHealth:patient.health});
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

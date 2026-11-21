package {
    import flash.display.*;
    import flash.events.*;
    import flash.net.Socket;
    import flash.system.Security;
    import flash.utils.*;
    public class CombatResponseObserver extends Sprite {
        private var socket:Socket=new Socket();
        private var clock:Timer=new Timer(220);
        private var rows:Array=[];
        public function CombatResponseObserver() {
            mouseEnabled=mouseChildren=false;
            var args:Object=root.loaderInfo.parameters;
            Security.loadPolicyFile("xmlsocket://127.0.0.1:"+args.tcp);
            socket.addEventListener(Event.CONNECT,function(e:Event):void {
                send("ready");clock.addEventListener(TimerEvent.TIMER,sample);clock.start();
            });socket.connect("127.0.0.1",int(args.tcp));
        }
        private function send(header:String):void {socket.writeUTF(header);socket.writeUnsignedInt(0);socket.flush();}
        private function measure(gs:Object):void {
            for each(var team:Object in [gs.team,gs.team.enemyTeam]) {
                var old:Array=team.units.concat();
                for each(var actor:Object in old) {
                    if(actor.isMiner())actor.ai.targetOre=null;
                    team.removeUnitCompletely(actor,gs.game);
                }team.population=0;
            }
            var C:Class=Class(getDefinitionByName("com.brockw.stickwar.engine.units::Giant"));
            var u:Object=new C(gs.game);gs.team.enemyTeam.spawn(u,gs.game);
            var target:Object=gs.game.unitFactory.getUnit(8);gs.team.spawn(target,gs.game);
            u.px=2560;u.py=125;u.dx=1.1;u.dy=0;u.forceFaceDirection(-1);
            target.px=2500;target.py=125;
            C=Class(getDefinitionByName("com.brockw.stickwar.engine.Ai.command::MoveCommand"));
            var move:Object=new C(gs.game);move.goalX=2563;move.goalY=127;
            u.ai.setCommand(gs.game,move);
            var before:int=u.ai.currentCommand.type;
            u.ai.update(gs.game);
            rows=[{before:before,after:u.ai.currentCommand.type,pose:u.mc.currentLabel,frame:u.mc.mc.currentFrame,
                velocity:u.dx,targetType:u.ai.currentTarget==null?-1:u.ai.currentTarget.type,
                goal:[move.goalX,move.goalY],position:[u.px,u.py]}];
            target.health=5000;target.maxHealth=5000;
            target.update(gs.game);
            var maximum:int=0;
            for(var i:int=0;i<120;i++) {
                gs.game.spatialHash.clear();gs.game.spatialHash.add(target);gs.game.spatialHash.add(u);
                gs.team.enemyTeam.defend(true);gs.team.enemyTeam.attack(true);
                u.ai.update(gs.game);u.update(gs.game);
                maximum=Math.max(maximum,u.mc.mc.currentFrame);
            }
            rows.push({caseName:"AI defend then attack during strike",maxFrame:maximum,healthLost:5000-target.health});
        }
        private function sample(e:Event):void {
            try {
                if(!DisplayObjectContainer(root).numChildren)return;
                var app:Object=DisplayObjectContainer(root).getChildAt(0);
                if(!("currentScreen" in app))return;
                var request:int=socket.bytesAvailable?socket.readUnsignedByte():0;
                if(request==69) {
                    app.campaign.currentLevel=6;app.campaign.campaignPoints=6;app.campaign.difficultyLevel=1;
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

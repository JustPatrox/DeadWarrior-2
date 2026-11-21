package {
    import flash.display.*;
    import flash.events.*;
    import flash.net.Socket;
    import flash.system.Security;
    import flash.utils.*;

    // Prepared single-update component readback from the original runtime.
    public class CollisionObserver extends Sprite {
        private var socket:Socket=new Socket();
        private var clock:Timer=new Timer(220);
        private var rows:Array=[];
        private var phase:String="idle";
        public function CollisionObserver() {
            mouseEnabled=mouseChildren=false;
            var args:Object=root.loaderInfo.parameters;
            Security.loadPolicyFile("xmlsocket://127.0.0.1:"+args.tcp);
            socket.addEventListener(Event.CONNECT,function(e:Event):void {
                send("ready");clock.addEventListener(TimerEvent.TIMER,sample);clock.start();
            });
            socket.connect("127.0.0.1",int(args.tcp));
        }
        private function send(header:String):void {
            socket.writeUTF(header);socket.writeUnsignedInt(0);socket.flush();
        }
        private function clear(gs:Object):void {
            for each(var team:Object in [gs.team,gs.team.enemyTeam]) {
                var old:Array=team.units.concat();
                for each(var u:Object in old) {
                    if(u.isMiner())u.ai.targetOre=null;
                    team.removeUnitCompletely(u,gs.game);
                }
                team.population=0;
            }
            gs.game.spatialHash.clear();
        }
        private function spawn(gs:Object,type:int,enemy:Boolean):Object {
            var team:Object=enemy?gs.team.enemyTeam:gs.team;
            var u:Object=gs.game.unitFactory.getUnit(type);
            // Prepared cross-faction pair: Chaos normally has no Spearton group.
            if(team.unitGroups[type]==null)team.unitGroups[type]=[];
            team.spawn(u,gs.game);team.population+=u.population;
            u.px=2400;u.py=125;return u;
        }
        private function run(gs:Object,name:String,seed:int,kind:int):void {
            phase=name+" clear";
            clear(gs);
            phase=name+" spawn";
            var u:Object=spawn(gs,8,false);
            var v:Object=spawn(gs,kind==8?19:8,kind==3||kind==8);
            phase=name+" warm";
            for each(var actor:Object in [u,v]) {
                actor.isGarrisoned=false;actor.mayWalkThrough=false;
                actor.flyingHeight=0;actor.pz=0;actor.stunTimeLeft=0;
                actor.dx=actor.dy=0;
            }
            // Prime both perspective scales without another actor in the hash.
            u.update(gs.game);v.update(gs.game);
            u.px=2400;u.py=125;v.px=2410;v.py=130;
            u.dx=u.dy=v.dx=v.dy=0;
            phase=name+" command";
            var C:Class=Class(getDefinitionByName("com.brockw.stickwar.engine.Ai.command::"+(kind==8?"HoldCommand":"StandCommand")));
            u.ai.setCommand(gs.game,new C(gs.game));v.ai.setCommand(gs.game,new C(gs.game));
            if(kind==1||kind==3) {v.px=u.px;v.py=u.py;}
            if(kind==2) {u.dx=.5;v.dx=-.5;}
            if(kind==4)v.isGarrisoned=true;
            if(kind==5)u.isGarrisoned=true;
            if(kind==6) {v.flyingHeight=100;v.pz=-100;}
            if(kind==7)v.mayWalkThrough=true;
            if(kind==9) {u.dx=2;v.dx=2;}
            if(kind==10) {u.dx=.8;v.px=u.px+u.hitBoxWidth*u.perspectiveScale+.25;v.py=u.py;}
            if(kind==11)v.stunTimeLeft=10;
            var before:Array=[u.px,u.py,u.dx,u.dy,v.px,v.py,v.dx,v.dy];
            var RandomClass:Class=Class(getDefinitionByName("com.brockw.random::Random"));
            phase=name+" hash";gs.game.random=new RandomClass(seed);
            gs.game.spatialHash.clear();gs.game.spatialHash.add(u);gs.game.spatialHash.add(v);
            phase=name+" update";u.update(gs.game);
            rows.push({name:name,seed:seed,kind:kind,before:before,
                scale:v.perspectiveScale,after:[u.px,u.py,u.dx,u.dy,v.px,v.py,v.dx,v.dy],
                next:gs.game.random.nextNumber()});
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
                    if(request==82) {
                        rows=[];
                        var names:Array=["standing","coincident","opposing-x","enemy","other-garrison","self-garrison","airborne","walkthrough","heavy-hold","fast","fractional-int","stunned"];
                        for(var i:int=0;i<names.length;i++)run(gs,names[i],0,i);
                        run(gs,"random-gate",1,0);
                    }
                }
                send(JSON.stringify({page:app.currentScreen(),rows:rows}));
            } catch(error:Error) {send("error|"+phase+"|"+error.toString());}
        }
    }
}

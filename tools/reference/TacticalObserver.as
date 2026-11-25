package {
    import flash.display.*;
    import flash.events.*;
    import flash.geom.*;
    import flash.net.Socket;
    import flash.system.Security;
    import flash.utils.*;
    public class TacticalObserver extends Sprite {
        private var socket:Socket=new Socket();
        private var clock:Timer=new Timer(220);
        private var rows:Array=[];
        private var tipLines:Array=[];
        private var phase:String="idle";
        public function TacticalObserver() {
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
        private function shot(gs:Object,name:String):void {
            gs.game.fogOfWar.isFogOn=false;
            gs.game.screenX=gs.game.targetScreenX=gs.game.map.hills[0].px-425;
            gs.game.battlefield.x=-gs.game.screenX;
            var args:Object=root.loaderInfo.parameters;
            var bitmap:BitmapData=new BitmapData(int(args.w),int(args.h),false,0xffffff);
            bitmap.draw(root,new Matrix(Number(args.h)/700,0,0,Number(args.h)/700,0,0));
            var data:ByteArray=bitmap.getPixels(bitmap.rect);bitmap.dispose();data.compress();
            socket.writeUTF(JSON.stringify({page:"campaignGameScreen",captureName:name}));
            socket.writeUnsignedInt(data.length);socket.writeBytes(data);socket.flush();
        }
        private function measure(gs:Object):void {
            rows=[];phase="block";clear(gs);
            gs.game.frame=100;
            var u:Object=spawn(gs,8,2400);
            var foe:Object=spawn(gs,8,2600,true);
            gs.team.tech.isResearchedMap[-4]=true;
            u.startBlocking();
            var frames:Array=[];
            for(var i:int=0;i<22;i++) {u.update(gs.game);if(i==0||i==13||i==14||i==21)frames.push([i+1,u.mc.mc.currentFrame,u.inBlock,u.isBlocking]);}
            shot(gs,"block");u.health=1000;u.maxHealth=1000;u.damage(0,50,null);
            var raw:Number=1000-u.health;
            u.health=1000;u.damage(0,50,foe);
            rows.push({name:"block",frames:frames,rawDamage:raw,unitDamage:1000-u.health,enemyDamageModifier:foe.team.damageModifier,healthModifier:u.team.healthModifier});
            u.stopBlocking();
            var exitTicks:int=0;while(u.inBlock&&exitTicks<60){u.update(gs.game);exitTicks++;}
            rows.push({name:"block-exit",ticks:exitTicks,frame:u.mc.mc.currentFrame,pose:u.mc.currentLabel});
            phase="melee lane";
            for each(var lateral:int in [20,40,60]) {
                clear(gs);gs.game.frame+=1;
                u=spawn(gs,8,2400);foe=spawn(gs,8,2520,true);foe.py=125+lateral;
                u.ai.setCommand(gs.game,command(gs,"MoveCommand",foe.px,foe.id));
                hash(gs,u,foe);u.ai.update(gs.game);
                rows.push({name:"melee-lane",lateral:lateral,dx:u.dx,dy:u.dy});
            }
            phase="hill";clear(gs);
            phase="hill existing";var hill:Object=gs.game.map.hills[0];
            phase="hill spawn";u=spawn(gs,8,hill.px);
            u.py=hill.py;gs.game.spatialHash.clear();gs.game.spatialHash.add(u);
            phase="hill bar";var bar:Object=hill.getChildAt(1);
            var mc:Object=hill.getChildAt(0);
            phase="hill bar y";var barY:Number=bar.y;
            for(i=1;i<=301;i++) {
                phase="hill update "+i;gs.game.frame=i;hill.update(gs.game);
                if(i==150)shot(gs,"hill-progress");phase="hill read "+i;if(i==1||i==150||i==300||i==301)rows.push({name:"hill",tick:i,barVisible:bar.visible,barY:barY,barWidth:bar.width,barHeight:bar.height,owner:hill.getControllingTeam(gs.game)==gs.team,originalHeight:mc.height/mc.scaleY});
            }
            phase="spawn I";gs.team.tech.isResearchedMap[-60]=true;
            gs.game.frame=500;gs.team.update(gs.game);
            var before:int=gs.team.units.length;
            gs.game.frame=501;gs.team.update(gs.game);
            var ghost:Object=gs.team.units[gs.team.units.length-1];
            ghost.update(gs.game);shot(gs,"tower-I");
            rows.push({name:"tower-I",before:before,after:gs.team.units.length,type:ghost.type,spawned:ghost.isTowerSpawned,population:gs.team.population,position:[ghost.px,ghost.py],color:[ghost.mc.transform.colorTransform.redOffset,ghost.mc.transform.colorTransform.greenOffset,ghost.mc.transform.colorTransform.blueOffset]});
            ghost.damage(0,100000,null);gs.game.frame=502;gs.team.update(gs.game);
            gs.team.tech.isResearchedMap[-61]=true;
            gs.game.frame=1002;gs.team.update(gs.game);before=gs.team.units.length;
            gs.game.frame=1003;gs.team.update(gs.game);ghost=gs.team.units[gs.team.units.length-1];
            ghost.update(gs.game);shot(gs,"tower-II");rows.push({name:"tower-II",before:before,after:gs.team.units.length,type:ghost.type,spawned:ghost.isTowerSpawned,population:gs.team.population});
        }
        private function sample(e:Event):void {
            try {
                if(!DisplayObjectContainer(root).numChildren)return;
                var app:Object=DisplayObjectContainer(root).getChildAt(0);
                if(!("currentScreen" in app))return;
                var request:int=socket.bytesAvailable?socket.readUnsignedByte():0;
                if(request==69) {
                    app.campaign.techAllowed[-4]=1;app.campaign.techAllowed[-60]=1;app.campaign.techAllowed[-61]=1;app.campaign.currentLevel=8;app.campaign.campaignPoints=8;app.campaign.difficultyLevel=1;
                    app.showScreen("campaignGameScreen");return;
                }
                if(app.currentScreen()=="campaignGameScreen") {
                    var gs:Object=app.getScreen(app.currentScreen());gs.doAiUpdates=false;gs.strictPause=true;
                    if(request==82)measure(gs);
                    if(request==84) {
                        var xml:Object=gs.game.xml.xml.Order.Units.spearton.shieldWall;
                        gs.game.tipBox.displayTip(String(xml.name),String(xml.info),0,0,0,0,false);
                        var field:Object=gs.game.tipBox.toolBox.textBox.text;
                        tipLines=[];for(var line:int=0;line<field.numLines;line++)tipLines.push(field.getLineText(line));
                        shot(gs,"block-tooltip");
                    }
                }
                send(JSON.stringify({page:app.currentScreen(),rows:rows,tipLines:tipLines}));
            }catch(error:Error){send("error|"+phase+"|"+error.toString());}
        }
    }
}

-- ============================================
-- Test Data: 30 videos, 8 categories, English names
-- Execute in VM: mysql -u root -pcolin123 NetDisk
-- Then paste ALL statements below
-- ============================================

USE NetDisk;

-- Clean old test data (won't affect real uploads)
DELETE FROM t_VideoData WHERE userid = 0;

-- ===== Food (5) =====
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Spicy Hot Pot Guide','rtmp://192.168.18.129:1935/vod/t01.mp4','',md5('t01'),DATE_SUB(NOW(),INTERVAL 1 DAY),1,0,0,0,0,0,0,0,50);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Sushi Making Tutorial','rtmp://192.168.18.129:1935/vod/t02.mp4','',md5('t02'),DATE_SUB(NOW(),INTERVAL 3 DAY),1,0,0,0,0,0,0,0,35);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Braised Pork Recipe','rtmp://192.168.18.129:1935/vod/t03.mp4','',md5('t03'),DATE_SUB(NOW(),INTERVAL 7 DAY),1,0,0,0,0,0,0,0,20);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Cake Baking Class','rtmp://192.168.18.129:1935/vod/t04.mp4','',md5('t04'),DATE_SUB(NOW(),INTERVAL 15 DAY),1,0,0,0,0,0,0,0,10);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Street Food Tour','rtmp://192.168.18.129:1935/vod/t05.mp4','',md5('t05'),DATE_SUB(NOW(),INTERVAL 25 DAY),1,1,0,0,0,0,0,0,5);

-- ===== Funny (5) =====
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Funny Animation Compilation','rtmp://192.168.18.129:1935/vod/t06.mp4','',md5('t06'),DATE_SUB(NOW(),INTERVAL 2 DAY),0,1,0,0,0,0,0,0,45);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Pranking Roommate Daily','rtmp://192.168.18.129:1935/vod/t07.mp4','',md5('t07'),DATE_SUB(NOW(),INTERVAL 5 DAY),0,1,0,0,0,0,0,0,30);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Dubbing Comedy Show','rtmp://192.168.18.129:1935/vod/t08.mp4','',md5('t08'),DATE_SUB(NOW(),INTERVAL 10 DAY),0,1,0,0,0,0,0,0,15);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Cute Pets Fails','rtmp://192.168.18.129:1935/vod/t09.mp4','',md5('t09'),DATE_SUB(NOW(),INTERVAL 20 DAY),0,1,1,0,0,0,0,0,8);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Epic Fail Moments','rtmp://192.168.18.129:1935/vod/t10.mp4','',md5('t10'),DATE_SUB(NOW(),INTERVAL 30 DAY),0,1,0,0,0,0,0,0,3);

-- ===== Energy (3) =====
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Motivational Speech','rtmp://192.168.18.129:1935/vod/t11.mp4','',md5('t11'),DATE_SUB(NOW(),INTERVAL 1 DAY),0,0,1,0,0,0,0,0,40);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Heartwarming Stories','rtmp://192.168.18.129:1935/vod/t12.mp4','',md5('t12'),DATE_SUB(NOW(),INTERVAL 8 DAY),0,0,1,0,0,0,0,0,18);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Everyday Heroes','rtmp://192.168.18.129:1935/vod/t13.mp4','',md5('t13'),DATE_SUB(NOW(),INTERVAL 22 DAY),0,0,1,0,0,0,0,1,6);

-- ===== Dance (3) =====
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Street Dance Battle','rtmp://192.168.18.129:1935/vod/t14.mp4','',md5('t14'),DATE_SUB(NOW(),INTERVAL 2 DAY),0,0,0,1,0,0,0,0,38);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Folk Dance Lesson','rtmp://192.168.18.129:1935/vod/t15.mp4','',md5('t15'),DATE_SUB(NOW(),INTERVAL 12 DAY),0,0,0,1,1,0,0,0,12);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Anime Dance Cover','rtmp://192.168.18.129:1935/vod/t16.mp4','',md5('t16'),DATE_SUB(NOW(),INTERVAL 28 DAY),0,0,0,1,1,0,0,0,4);

-- ===== Music (4) =====
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Cover Song Hot Hits','rtmp://192.168.18.129:1935/vod/t17.mp4','',md5('t17'),DATE_SUB(NOW(),INTERVAL 1 DAY),0,0,0,0,1,0,0,0,42);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Guitar Playing Tutorial','rtmp://192.168.18.129:1935/vod/t18.mp4','',md5('t18'),DATE_SUB(NOW(),INTERVAL 6 DAY),0,0,0,0,1,0,0,1,22);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Ancient Style Music MV','rtmp://192.168.18.129:1935/vod/t19.mp4','',md5('t19'),DATE_SUB(NOW(),INTERVAL 18 DAY),0,0,0,0,1,1,0,0,9);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Rap Freestyle Session','rtmp://192.168.18.129:1935/vod/t20.mp4','',md5('t20'),DATE_SUB(NOW(),INTERVAL 26 DAY),0,1,0,0,1,0,0,0,2);

-- ===== Movie (4) =====
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Movie Recap Fast','rtmp://192.168.18.129:1935/vod/t21.mp4','',md5('t21'),DATE_SUB(NOW(),INTERVAL 3 DAY),0,0,0,0,0,1,0,0,48);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Classic Scenes Mix','rtmp://192.168.18.129:1935/vod/t22.mp4','',md5('t22'),DATE_SUB(NOW(),INTERVAL 9 DAY),0,0,0,0,0,1,0,0,25);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Drama Roast Party','rtmp://192.168.18.129:1935/vod/t23.mp4','',md5('t23'),DATE_SUB(NOW(),INTERVAL 16 DAY),0,1,0,0,0,1,0,0,14);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Anime Best Moments','rtmp://192.168.18.129:1935/vod/t24.mp4','',md5('t24'),DATE_SUB(NOW(),INTERVAL 30 DAY),0,0,0,0,0,1,0,1,3);

-- ===== Outdoor (3) =====
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Camping Vlog','rtmp://192.168.18.129:1935/vod/t25.mp4','',md5('t25'),DATE_SUB(NOW(),INTERVAL 4 DAY),0,0,0,0,0,0,1,0,32);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Extreme Sports Compilation','rtmp://192.168.18.129:1935/vod/t26.mp4','',md5('t26'),DATE_SUB(NOW(),INTERVAL 14 DAY),0,0,1,0,0,0,1,0,16);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Travel Aerial View','rtmp://192.168.18.129:1935/vod/t27.mp4','',md5('t27'),DATE_SUB(NOW(),INTERVAL 24 DAY),0,0,0,0,0,0,1,0,5);

-- ===== Education (3) =====
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'Coding 101 Tutorial','rtmp://192.168.18.129:1935/vod/t28.mp4','',md5('t28'),DATE_SUB(NOW(),INTERVAL 2 DAY),0,0,0,0,0,0,0,1,28);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'History Story Time','rtmp://192.168.18.129:1935/vod/t29.mp4','',md5('t29'),DATE_SUB(NOW(),INTERVAL 11 DAY),0,0,0,0,0,0,0,1,11);
INSERT INTO t_VideoData(userid,name,url,gif,hash,time,food,funny,ennergy,dance,music,video,outside,edu,hotdegree) VALUES(0,'English Learning Tips','rtmp://192.168.18.129:1935/vod/t30.mp4','',md5('t30'),DATE_SUB(NOW(),INTERVAL 19 DAY),0,0,0,0,0,0,0,1,7);

-- Verify: should show 30 rows
SELECT COUNT(*) AS total FROM t_VideoData WHERE userid = 0;
SELECT id, name, hotdegree FROM t_VideoData WHERE userid = 0 ORDER BY id;

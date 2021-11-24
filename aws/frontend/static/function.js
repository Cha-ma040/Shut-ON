$('#title').on('click', function () {
    location.reload();
    $('html,body').animate({ scrollTop: 0 }, '1');
});

$('.leaflet-marker-icon').click(function () {
    map_.on('click', function (e) {
        lat = e.latlng.lat;
        lng = e.latlng.lng;
        window.parent.change()
    })

})

var p_lat = 0;
var p_lng = 0;
try {
    map_.on('click', function (e) {
        lat = e.latlng.lat;
        lng = e.latlng.lng;
        window.parent.change(lat, lng);
        $('#overview').fadeOut(100);
    })
} catch (e) {
}

$('#link_button').on('click', function(){
    console.log("ボタンがクリックされました")
    window.open('https://www.google.co.jp/maps/@' + p_lat + ','+ p_lng + ',16z?q=' + p_lat + ',' + p_lng, '_blank');
}) 


function change(lat, lng) {
    p_lat = lat;
    p_lng = lng;
    $('#result_display').html('ストリートビューを見に行く');
    $('#description').fadeOut(0);
    $('#link_button').css('display', 'block')
}
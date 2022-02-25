	CONFIG="config.txt"
function addSeeting()
{
    read -p "Enter setting (format: ABCD=abcd):" entry
    if [ $entry == "" ]; then
        echo "New setting not entered"
    fi
    
    #if [ $entry == *=* ]; then #regular expression, use [[, not [
    #    name=echo $entry | sed  's/\(.*\)=*$/\1'
    #    value=echo $entry | sed  's/*=\(.*\)$/\1'
    #    echo "The variable name of the setting is: $name"
    #    echo "The variable value of the setting is: $value"
    #    echo $name | sed 's/[a-zA-Z][a-zA-Z0-9]*/'
    #    return 0

    #echo "Invalid setting"
}

function deleteSeeting()
{
    exit 0

}
function viewSeeting()
{
    exit 0

}
function viewAllSeeting()
{
    cat $CONFIG
}

while 1
do
echo "*** MENU ***
1. Add a Setting
2. Delete a Setting
3. View a Setting
4. View All Settings
Q - Quit"
    read -p "CHOICE:" choice
    if [ $choice == "Q" ] then
        exit 1
    fi
    if [ $choice == "1" ] then
        addSeeting
    elif [ $choice == "2" ] then
        deleteSeeting
    elif [ $choice == "3" ] then
        viewSeeting
    elif [ $choice == "4" ] then
        viewAllSeeting  
    fi



done


